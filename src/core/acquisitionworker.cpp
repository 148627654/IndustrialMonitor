#include "acquisitionworker.h"
#include <QDebug>
#include <devicefactory.h>
AcquisitionWorker::AcquisitionWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_intervalMs(1000)
    , m_pollTimer(nullptr)
    , m_tickCount(0)
{
    qInfo() << "[AcquisitionWorker] Instantiated in Thread:" << QThread::currentThread();
}

AcquisitionWorker::~AcquisitionWorker()
{
    stopWork();
    qInfo() << "[AcquisitionWorker] Destroyed in Thread:" << QThread::currentThread();
}

void AcquisitionWorker::startWork(const QList<DeviceInfo> &devices)
{
    if(m_running)
        return;
    m_running=true;

    qInfo() << "[AcquisitionWorker] startWork() in Thread:" << QThread::currentThread();

    // 在子线程内部加载并开启所以驱动通道
    loadDrivers(devices);

    // QTimer 必须在子线程上下文中延迟实例化！
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &AcquisitionWorker::onPollTimeout);
    }
    m_pollTimer->start(m_intervalMs);

    emit statusMessage("Acquisition Worker engine and driver pool started.");
    emit started();
    // ==================== [V3-Day 2 验证测试：从子线程构造并向外发射一帧数据] ====================
    // TelemetryFrame mockFrame;
    // mockFrame.deviceId = "DEV-1001";
    // mockFrame.timestamp = QDateTime::currentDateTime();
    // mockFrame.temperature = 42.5;
    // mockFrame.cpuUsage = 28.0;
    // mockFrame.memoryUsage = 35.0;
    // mockFrame.commSuccess = true;
    // mockFrame.rawRegisters[0] = 425; // 42.5 * 10

    // qInfo() << "[AcquisitionWorker] Dispatching mock TelemetryFrame from Worker Thread...";
    // emit telemetryDataReady(mockFrame);

}

void AcquisitionWorker::stopWork()
{
    if(!m_running)
        return;
    m_running=false;
    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer; // 加上这行彻底消除跨线程 killTimer 警告！
        m_pollTimer = nullptr;
    }

    unloadDrivers();

    qInfo() << "[AcquisitionWorker] Stopped in Thread:" << QThread::currentThread();
    emit statusMessage("Acquisition Worker stopped.");
    emit stopped();
}

void AcquisitionWorker::setInterval(int ms)
{
    if (ms <= 0) return;
    m_intervalMs = ms;
    if (m_pollTimer && m_pollTimer->isActive()) {
        m_pollTimer->setInterval(m_intervalMs);
    }
}

void AcquisitionWorker::onPollTimeout()
{
    if (!m_running || m_drivers.empty()) return;
    m_tickCount++;

    // 获取当前物理时间戳 (毫秒)，用于退避时间计算
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    processReconnections(nowMs);

    QList<TelemetryFrame> batch;
    batch.reserve(static_cast<int>(m_drivers.size()));

    for (auto &[id, driver] : m_drivers)
    {
        TelemetryFrame frame;
        frame.deviceId  = id;
        frame.timestamp = QDateTime::currentDateTime();

        // 检查驱动实例有效性与连接健康度
        if (driver && driver->isConnected())
        {
            QList<quint16> rawBuffer;
            // 尝试读取工控保持寄存器
            if (driver->readRegisters(0, 3, rawBuffer) && rawBuffer.size() >= 3)
            {
                // ① 物理量定标解码 (必须除以 10.0 浮点常量，严防整数除法截断小数位)
                frame.temperature = rawBuffer[0] / 10.0;
                frame.cpuUsage    = qBound(0.0, rawBuffer[1] / 10.0, 100.0);

                // 将电压(约380V)换算映射为 0~100% 内存负荷指标
                double memoryUse  = (rawBuffer[2] - 3600.0) / 4.0;
                frame.memoryUsage = qBound(10.0, memoryUse, 95.0);
                frame.commSuccess = true;

                // ② 写入 16 位原始寄存器快照 (供底层排障溯源)
                frame.rawRegisters[0] = rawBuffer[0];
                frame.rawRegisters[1] = rawBuffer[1];
                frame.rawRegisters[2] = rawBuffer[2];
            }
            else
            {
                // 通信读取异常 (如物理线缆脱落、校验错误)
                frame.commSuccess = false;
                // 核心接入：触发指数退避重连流水线！
                triggerReconnection(id, nowMs);
            }
        }
        else
        {
            // 设备未连接或正处于 Reconnecting / Error 状态
            frame.commSuccess = false;
            // 如果设备不在重连调度队列中，确保将其加入重连状态机
            triggerReconnection(id, nowMs);
        }

        batch.append(frame);

        // 无锁原子发射计数自增
        m_framesEmitted.fetch_add(1, std::memory_order_relaxed);

        // 单帧信号分发 (保持向下兼容)
        emit telemetryDataReady(frame);
    }

    // ========================================================
    // 步骤 3：原子发射全厂批量遥测包
    // ========================================================
    emit telemetryBatchReady(batch);
}

void AcquisitionWorker::loadDrivers(const QList<DeviceInfo> &devices)
{
    unloadDrivers();

    qInfo() << QString("[AcquisitionWorker] Assembling driver pool for %1 devices in Thread:")
                   .arg(devices.size()) << QThread::currentThread();

    for(const auto &devInfo:devices)
    {
        // 利用工厂实现实例化驱动
        std::unique_ptr<IDevice> driver=DeviceFactory::createDevice(devInfo);
        if(driver)
        {
            connect(driver.get(),&IDevice::stateChanged,this,[this,id=devInfo.id](IDevice::ConnectState statue){
                emit deviceStateChanged(id,statue);
            });
            m_drivers[devInfo.id]=std::move(driver);
        }
    }
    qInfo() << QString("[AcquisitionWorker] Successfully assembled %1 active drivers. Opening channels...").arg(m_drivers.size());

    // 批量并发打开通信通道
    for (auto &[id, driver] : m_drivers) {
        driver->open(); // 多态调用！
    }
}

void AcquisitionWorker::unloadDrivers()
{
    if(m_drivers.empty())
        return;
    qInfo() << QString("[AcquisitionWorker] Closing and releasing %1 hardware drivers...").arg(m_drivers.size());
    for(auto &[id,driver]:m_drivers)
        driver->close();
    m_drivers.clear();
    // 清空遗留的重连上下文，杜绝跨 start/stop 周期的脏状态
    m_reconnectContexts.clear();
}

void AcquisitionWorker::processReconnections(qint64 nowMs)
{
    for(auto &[id,ctx]:m_reconnectContexts)
    {
        if(!ctx.isRetrying)
            continue;
        if(nowMs<ctx.nextRetryTimestampMs)
            continue;
        auto it=m_drivers.find(id);
        if(it==m_drivers.end()||!it->second)
            continue;
        auto &driver=it->second;
        ctx.retryCount++;
        qInfo() << QString("[Reconnector] Device [%1] attempting reconnect (Attempt %2/%3)...")
                       .arg(id).arg(ctx.retryCount).arg(MAX_RETRY_ATTEMPTS);
        if (driver->open()) {
            qInfo() << QString("[Reconnector] Device [%1] Link Restored successfully! Self-healing completed.")
            .arg(id);
            ctx.isRetrying = false;
            ctx.retryCount = 0;
            // 驱动内部会自动发射 Connected 信号同步状态
        } else {
            // 重连仍然失败
            if (ctx.retryCount >= MAX_RETRY_ATTEMPTS) {
                qWarning() << QString("[Reconnector] Device [%1] Max retries (%2) exceeded! Transition to Error state.")
                .arg(id).arg(MAX_RETRY_ATTEMPTS);
                ctx.isRetrying = false; // 停止重试
                driver->setState(IDevice::ConnectState::Error);
            } else {
                // 计算下一次指数退避时延 (base * 2^retry)
                int backoff = qMin(BASE_BACKOFF_MS * (1 << ctx.retryCount), MAX_BACKOFF_MS);
                ctx.nextRetryTimestampMs = nowMs + backoff;
                qInfo() << QString("[Reconnector] Device [%1] Next retry scheduled in %2 ms.").arg(id).arg(backoff);
            }
        }
    }
}

void AcquisitionWorker::injectFault(const QString &deviceId, bool faultOn)
{
    auto it=m_drivers.find(deviceId);
    if(it==m_drivers.end()||!it->second)
    {
        qWarning() << QString("[FaultInjector] Device [%1] not found in driver pool!").arg(deviceId);
        return;
    }

    // 多态路由到驱动级故障钩子 (MockDevice 强制断链，真实驱动默认空实现)
    it->second->injectFault(faultOn);
    qInfo() << QString("[FaultInjector] Fault %1 on [%2].")
                   .arg(faultOn?"INJECTED":"CLEARED").arg(deviceId);

    if(!faultOn)
    {
        // 故障排除：若驱动不在线且已退出退避观察期 (如耗尽重试进入 Error)，立即重启自愈序列
        auto ctxIt=m_reconnectContexts.find(deviceId);
        const bool retrying=(ctxIt!=m_reconnectContexts.end()&&ctxIt->second.isRetrying);
        if(!it->second->isConnected()&&!retrying)
            triggerReconnection(deviceId,QDateTime::currentMSecsSinceEpoch());
    }
}

void AcquisitionWorker::triggerReconnection(const QString &deviceId, qint64 nowMs)
{
    auto &ctx=m_reconnectContexts[deviceId];
    if(ctx.isRetrying)// 已经在队列中
        return;
    ctx.isRetrying=true;
    ctx.retryCount=0;
    ctx.nextRetryTimestampMs=nowMs+BASE_BACKOFF_MS;

    auto it=m_drivers.find(deviceId);
    if(it!=m_drivers.end()&&it->second)
        it->second->setState(IDevice::ConnectState::Connecting);

    qWarning() << QString("[Reconnector] Connection lost on [%1]! Initiating backoff sequence. First retry in %2 ms.")
                      .arg(deviceId).arg(BASE_BACKOFF_MS);
}
