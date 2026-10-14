#include "acquisitioncontroller.h"

#include "acquisitionworker.h"

#include <QSqlQuery>
#include <databasemanager.h>
#include <devicedao.h>

AcquisitionController::AcquisitionController(QObject *parent)
    : QObject(parent)
{
}

AcquisitionController::~AcquisitionController()
{
    stop();
}

AcquisitionController& AcquisitionController::instance()
{
    static AcquisitionController instance;
    return instance;
}

bool AcquisitionController::init()
{
    if (m_initialized) return true;

    // 1. 创建物理子线程与工作者
    m_thread = new QThread(this);
    m_worker = new AcquisitionWorker(); // 严禁设置 parent

    // 2. 转移线程亲和性
    m_worker->moveToThread(m_thread);

    // 3. 控制信号跨线程绑定
    // connect(m_thread, &QThread::started,  m_worker, &AcquisitionWorker::startWork);
    connect(m_worker, &AcquisitionWorker::started, this, &AcquisitionController::engineStarted);
    connect(m_worker, &AcquisitionWorker::stopped, this, &AcquisitionController::engineStopped);

    // 仅负责无锁计数统计 (真实透传由下方信号-信号直连完成，避免一帧被双重发射)
    connect(m_worker,&AcquisitionWorker::telemetryDataReady,this,[this](const TelemetryFrame &frame){
        Q_UNUSED(frame)
        m_framesReceived.fetch_add(1,std::memory_order_relaxed);
    });

    // 4. 信号中继透传
    connect(m_worker, &AcquisitionWorker::telemetryDataReady,  this, &AcquisitionController::telemetryDataReady);
    // connect(m_worker, &AcquisitionWorker::telemetryBatchReady, this, &AcquisitionController::telemetryBatchReady);
    connect(m_worker, &AcquisitionWorker::statusMessage,       this, &AcquisitionController::statusMessage);

    m_initialized = true;
    qInfo() << "[AcquisitionController] Initialized successfully. Worker moved to Thread.";

    // 初始化节流定时器
    if(!m_throttleTimer)
    {
        m_throttleTimer=new QTimer(this);
        connect(m_throttleTimer,&QTimer::timeout,this,&AcquisitionController::onThrottleTimeout);
    }
    connect(m_worker, &AcquisitionWorker::telemetryBatchReady, this, &AcquisitionController::onWorkerBatchReady);
    return true;
}

void AcquisitionController::stop()
{
    if(m_throttleTimer&&m_throttleTimer->isActive())
        m_throttleTimer->stop();
    m_pendingFrames.clear();
    // 如果已经处于停止状态或未初始化，直接返回
    if (m_state == EngineState::Stopped || !m_thread) {
        return;
    }

    if (m_state == EngineState::Stopping) {
        qWarning() << "[AcquisitionController] Stop already in progress!";
        return;
    }

    m_state = EngineState::Stopping;
    qInfo() << "[AcquisitionController] Graceful shutdown initiated...";

    // 1. 核心修改：用 BlockingQueuedConnection 阻塞等待子线程彻底停掉并销毁定时器！
    if (m_worker) {
        m_lastEmittedCount = m_worker->framesEmitted();
        const bool invoked = QMetaObject::invokeMethod(m_worker, "stopWork", Qt::BlockingQueuedConnection);
        if (!invoked) {
            qWarning() << "[AcquisitionController] stopWork invocation failed (worker not reachable).";
        }
    }

    // 2. 此时定时器已在子线程安全销毁，放心退出子线程
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        if (!m_thread->wait(2000)) {
            // 兜底防御：等待超时严禁直接 delete 运行中的线程 (会导致 UB/崩溃)
            qCritical() << "[AcquisitionController] Worker thread failed to exit within 2s. Force terminating...";
            m_thread->terminate();
            m_thread->wait();
        }
    }

    // 3. 释放 worker 和 thread
    if (m_worker) {
        delete m_worker;
        m_worker = nullptr;
    }
    if (m_thread) {
        delete m_thread;
        m_thread = nullptr;
    }
    m_initialized = false;

    m_state = EngineState::Stopped;
    qInfo() << "[AcquisitionController] Engine stopped safely. Thread joined.";
    emit engineStopped();
}
void AcquisitionController::start(int intervalMs)
{
    if (!m_initialized) {
        init();
    }

    if (m_state!=EngineState::Stopped) {
        qWarning() << "[AcquisitionController] Start rejected: Engine is currently"<<static_cast<int>(m_state);
        return;
    }

    m_state=EngineState::Starting;
    // 若未显式传入周期，从 SQLite t_sys_config 自动读取
    int targetInterval = (intervalMs > 0) ? intervalMs : loadDefaultIntervalFromDb();

    QList<DeviceInfo> dbDevices=DeviceDao::queryAllDevices();
    qInfo() << QString("[AcquisitionController] Loaded %1 devices from SQLite. Dispatching to Worker...").arg(dbDevices.size());

    m_thread->start(); // 启动线程，自动触发 worker->startWork()

    QMetaObject::invokeMethod(m_worker, "setInterval", Qt::QueuedConnection, Q_ARG(int, targetInterval));
    QMetaObject::invokeMethod(m_worker, "startWork", Qt::QueuedConnection, Q_ARG(QList<DeviceInfo>, dbDevices));

    m_state=EngineState::Running;
    if(m_throttleEnabled&&m_throttleTimer&&!m_throttleTimer->isActive())
        m_throttleTimer->start(m_throttleIntervalMs);
}

void AcquisitionController::setInterval(int intervalMs)
{
    if (intervalMs <= 0 || !m_worker) return;
    // 跨线程调用 Worker 的调频槽
    QMetaObject::invokeMethod(m_worker, "setInterval", Qt::QueuedConnection, Q_ARG(int, intervalMs));
}

bool AcquisitionController::isRunning() const
{
    return m_thread && m_thread->isRunning();
}

quint64 AcquisitionController::framesEmitted() const {
    return m_worker ? m_worker->framesEmitted() : m_lastEmittedCount;
}

void AcquisitionController::resetStatistics()
{
    m_framesReceived.store(0,std::memory_order_relaxed);
    if(m_worker)
        m_worker->resetStatistics();
}

void AcquisitionController::setThrottleEnabled(bool enable)
{
    m_throttleEnabled=enable;

    if(!enable)
    {
        if(m_throttleTimer&&m_throttleTimer->isActive())
            m_throttleTimer->stop();
        // 停表前立即冲刷合并桶，防止缓冲区中已合并的最新帧永远丢失
        if(!m_pendingFrames.isEmpty())
        {
            QList<TelemetryFrame> restFrames=m_pendingFrames.values();
            m_pendingFrames.clear();
            emit throttledBatchReady(restFrames);
        }
    }
    else if(enable&&m_state==EngineState::Running&&m_throttleTimer&&!m_throttleTimer->isActive())
        m_throttleTimer->start(m_throttleIntervalMs);
}

void AcquisitionController::setThrottleInterval(int ms)
{
    // 按任务书要求锁定视口刷新窗口 16ms ~ 100ms (60 FPS ~ 10 FPS)
    m_throttleIntervalMs=qBound(16,ms,100);
    if(m_throttleTimer&&m_throttleTimer->isActive())
        m_throttleTimer->setInterval(m_throttleIntervalMs);
}

void AcquisitionController::onWorkerBatchReady(const QList<TelemetryFrame> &batch)
{
    emit telemetryBatchReady(batch);
    if(!m_throttleEnabled)
    {
        emit throttledBatchReady(batch);
        return;
    }
    for(const auto &frame:batch)
        m_pendingFrames.insert(frame.deviceId,frame);
}

void AcquisitionController::onThrottleTimeout()
{
    if(m_pendingFrames.isEmpty())
        return;
    QList<TelemetryFrame> mergedBatch=m_pendingFrames.values();
    m_pendingFrames.clear();

    emit throttledBatchReady(mergedBatch);
}

int AcquisitionController::loadDefaultIntervalFromDb()
{
    int interval = 1000; // 默认缺省 1 秒
    QSqlDatabase db = DatabaseManager::instance().database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT f_value FROM t_sys_config WHERE f_key = 'Telemetry_Interval_Ms' LIMIT 1;");
        if (q.exec() && q.next()) {
            int dbVal = q.value(0).toInt();
            if (dbVal > 0) {
                interval = dbVal;
                qInfo() << "[AcquisitionController] Loaded telemetry interval from DB:" << interval << "ms";
                return interval;
            }
        }
    }
    qInfo() << "[AcquisitionController] Using fallback interval:" << interval << "ms";
    return interval;
}
