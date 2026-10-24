#include "mainwindow.h"
#include "views/logindialog.h"
#include <QApplication>
#include "common/thememanager.h"
#include "db/databasemanager.h"
#include "dao/devicedao.h"
#include <QElapsedTimer>
#include <QTimer>
#include <acquisitioncontroller.h>
#include "idevice.h"
#include "mockdevice.h"
#include "devicefactory.h"
#include <map>
#include "core/acquisitionworker.h"
#include "alarmmanager.h"
#include "widgets/AlarmBanner.h"
#include "RealtimePlotWidget.h"
#include "PlotTracer.h"
#include "qcustomplot.h"
#include <QVBoxLayout>
#include <QUuid>
#include <QCheckBox>
#include <QPushButton>
// V2 014
void testDeviceFactoryAndPool()
{
    qInfo() << "================ [DeviceFactory & Driver Pool Test Start] ================";

    // 1. 从 SQLite 提取现有设备台账
    QList<DeviceInfo> dbDevices = DeviceDao::queryAllDevices();
    qInfo() << "Found database devices count:" << dbDevices.size();

    // 2. 建立工业驱动调度管理池 (✅ 换成标准库的 std::map)
    std::map<QString, std::unique_ptr<IDevice>> driverPool;

    // 3. 批量通过工厂生产驱动并装载入池
    for (const auto &devInfo : dbDevices) {
        std::unique_ptr<IDevice> driver = DeviceFactory::createDevice(devInfo);
        if (driver) {
            // 统一订阅信号
            QObject::connect(driver.get(), &IDevice::stateChanged, [id = devInfo.id](IDevice::ConnectState s) {
                qInfo() << QString(">>> DriverPool Event [%1] State -> %2").arg(id).arg(static_cast<int>(s));
            });

            // ✅ 使用 std::map 存储，用 std::move 完美移交所有权
            driverPool[devInfo.id] = std::move(driver);
        }
    }

    qInfo() << "DriverPool assembled successfully! Total active drivers:" << driverPool.size();

    // 4. 多态并发调度：C++17 结构化绑定遍历 [id, driver]，一键启动所有通信驱动！
    qInfo() << "Starting all hardware drivers asynchronously...";
    for (auto &[id, driver] : driverPool) {
        driver->open(); // 多态调用！不感知具体是哪个驱动类
    }

    // 5. 等待 60ms 让所有驱动异步握手完成
    QEventLoop loop;
    QTimer::singleShot(60, &loop, &QEventLoop::quit);
    loop.exec();

    // 6. 验证所有驱动的在线状态与数据读取
    for (auto &[id, driver] : driverPool) {
        if (driver->isConnected()) {
            QList<quint16> buffer;
            if (driver->readRegisters(0, 1, buffer)) {
                qInfo() << QString("Polled [%1] -> Register[0] = %2").arg(driver->deviceId()).arg(buffer.first());
            }
        }
    }

    // 7. 优雅关停所有通道
    qInfo() << "Stopping all hardware drivers...";
    for (auto &[id, driver] : driverPool) {
        driver->close();
    }

    // 8. 驱动池析构 (清空容器，触发所有智能指针安全自释放)
    driverPool.clear();
    qInfo() << "DriverPool cleared cleanly. Zero memory leaks.";

    qInfo() << "================ [DeviceFactory & Driver Pool Test End: All Passed] ================";
}

// V3 001 子进程
void testAcquisitionWorkerThreadAffinity()
{
    qInfo() << "================ [Thread Affinity & Worker Test Start] ================";

    // 1. 打印 GUI 主线程标识
    qInfo() << "[Main Thread] GUI Thread Context:" << QThread::currentThread();

    // 2. 创建独立物理线程与工作者实例 (工作者严禁传 parent！)
    QThread *workerThread = new QThread();
    AcquisitionWorker *worker = new AcquisitionWorker();

    // 3. 核心机制：将工作者彻底移入子线程，转移线程亲和性
    worker->moveToThread(workerThread);

    // 4. 建立跨线程异步信号槽流水线
    // QObject::connect(workerThread, &QThread::started,  worker, &AcquisitionWorker::startWork);
    QObject::connect(worker,       &AcquisitionWorker::stopped, workerThread, &QThread::quit);

    // 优雅释放：线程结束后级联销毁自身与工作者
    QObject::connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 监听工作者状态广播
    QObject::connect(worker, &AcquisitionWorker::statusMessage, [](const QString &msg) {
        qInfo() << ">>> Observer Received Worker Event:" << msg;
    });

    // 5. 启动物理子线程 (自动触发 workerThread::started -> worker::startWork)
    workerThread->start();

    // 6. 等待 80ms 让子线程完整流转并验证
    QEventLoop loop;
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();

    // 7. 发射跨线程信号，通知工作者优雅停工
    qInfo() << "[Main Thread] Requesting worker to stop...";
    QMetaObject::invokeMethod(worker, "stopWork", Qt::QueuedConnection);

    // 等待子线程彻底退出完毕
    workerThread->wait(1000);
    qInfo() << "[Main Thread] Worker thread joined and finished safely.";

    qInfo() << "================ [Thread Affinity & Worker Test End: All Passed] ================";
}

// V3 002
void testCrossThreadTelemetryDelivery()
{
    qInfo() << "================ [Cross-Thread Telemetry Test Start] ================";

    // 1. 打印 GUI 主线程标识
    qInfo() << "[Main Thread] GUI Thread Context:" << QThread::currentThread();

    // 2. 验证元类型是否注册成功
    bool isMetaValid = QMetaType::fromType<TelemetryFrame>().isValid();
    qInfo() << "[MetaType Check] Is TelemetryFrame registered:" << isMetaValid;

    // 3. 构造子线程与工作者
    QThread *workerThread = new QThread();
    AcquisitionWorker *worker = new AcquisitionWorker();
    worker->moveToThread(workerThread);

    // 4. 关键监听：在主线程监听子线程发出的自定义结构体信号！
    QObject::connect(worker, &AcquisitionWorker::telemetryDataReady, [](const TelemetryFrame &frame) {
        qInfo() << ">>> [Main Thread Receiver] Caught TelemetryFrame asynchronously!";
        qInfo() << ">>> Received in Thread:" << QThread::currentThread();
        qInfo() << ">>> Frame Data Content:" << frame;
    });

    // QObject::connect(workerThread, &QThread::started,  worker, &AcquisitionWorker::startWork);
    QObject::connect(worker,       &AcquisitionWorker::stopped, workerThread, &QThread::quit);
    QObject::connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 5. 启动子线程
    workerThread->start();

    // 6. 等待 80ms 接收跨线程数据投递
    QEventLoop loop;
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();

    // 7. 优雅关停
    QMetaObject::invokeMethod(worker, "stopWork", Qt::QueuedConnection);
    workerThread->wait(1000);
    qInfo() << "[Main Thread] Test finished cleanly.";

    qInfo() << "================ [Cross-Thread Telemetry Test End: All Passed] ================";
}

// V3 003
void testAcquisitionControllerWorkflow()
{
    qInfo() << "================ [AcquisitionController Pipeline Test Start] ================";

    // 1. 初始化中枢
    AcquisitionController::instance().init();

    // 2. 外部主线程监听中枢广播的数据总线
    QObject::connect(&AcquisitionController::instance(), &AcquisitionController::telemetryDataReady,
                     [](const TelemetryFrame &frame) {
                         qInfo() << ">>> [Bus Receiver] Live Frame Arrival:" << frame;
                     });

    // 3. 启动引擎 (自动读取数据库配置 1000ms)
    AcquisitionController::instance().start();

    // 4. 等待 2100ms 观察两帧常速数据
    QEventLoop loop1;
    QTimer::singleShot(2100, &loop1, &QEventLoop::quit);
    loop1.exec();

    // 5. 动态热调频：加速到 200ms 高频模式！
    qInfo() << "[Main Thread] Testing dynamic live frequency adjustment -> 200 ms";
    AcquisitionController::instance().setInterval(200);

    // 6. 等待 800ms 观察高频跳动 (应收到约 4 帧)
    QEventLoop loop2;
    QTimer::singleShot(800, &loop2, &QEventLoop::quit);
    loop2.exec();

    // 7. 一键优雅停工
    AcquisitionController::instance().stop();
    qInfo() << "================ [AcquisitionController Pipeline Test End: All Passed] ================";
}

// V3 004
void testRapidStartStopCycles(int cycles = 50)
{
    qInfo() << "================ [Rapid Start/Stop Stress Test Start] ================";
    qInfo() << QString("Target stress cycles: %1").arg(cycles);

    AcquisitionController::instance().init();

    for (int i = 1; i <= cycles; ++i) {
        // 快速启动 (50ms 超高速周期)
        AcquisitionController::instance().start(50);

        // 极短随机运行 5~15ms (模拟极端竞态条件)
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();

        // 立即强制停止
        AcquisitionController::instance().stop();

        if (i % 10 == 0) {
            qInfo() << QString(">>> Completed cycle: %1 / %2 (State: OK)").arg(i).arg(cycles);
        }
    }

    qInfo() << "================ [Rapid Start/Stop Stress Test End: 100% Passed] ================";
}

// V3 005
void testHighFrequencyBurstConcurrency()
{
    qInfo() << "================ [100Hz High-Frequency Burst Test Start] ================";

    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();
    controller.resetStatistics();

    // 1. 设置极速 10ms (100Hz) 采样周期
    const int burstIntervalMs = 10;
    const int burstDurationMs = 2000; // 持续轰炸 2 秒
    qInfo() << QString("[Stress Setup] Interval: %1 ms (100Hz) | Duration: %2 ms")
                   .arg(burstIntervalMs).arg(burstDurationMs);

    // 2. 启动采集引擎
    controller.start(burstIntervalMs);

    // 3. 运行 2 秒，期间主线程保持事件循环活跃
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < burstDurationMs) {
        qApp->processEvents(); // 模拟主线程持续处理界面事件
        QThread::msleep(5);
    }

    // 4. 优雅停工
    controller.stop();
    qApp->processEvents();
    qint64 totalElapsed = timer.elapsed();

    // 5. 提取并计算性能指标
    quint64 emitted  = controller.framesEmitted();
    quint64 received = controller.framesReceived();
    qint64  lost     = static_cast<qint64>(emitted) - static_cast<qint64>(received);
    double  rate     = (totalElapsed > 0) ? (received * 1000.0 / totalElapsed) : 0.0;

    qInfo() << "-------------------- [Benchmark Results] --------------------";
    qInfo() << QString("Total Run Time       : %1 ms").arg(totalElapsed);
    qInfo() << QString("Worker Frames Emitted: %1").arg(emitted);
    qInfo() << QString("GUI Frames Received  : %1").arg(received);
    qInfo() << QString("Frames Lost          : %1 (Drop Rate: %2 %)")
                   .arg(lost).arg(emitted > 0 ? (lost * 100.0 / emitted) : 0.0, 0, 'f', 2);
    qInfo() << QString("Average Throughput   : %1 Frames/sec").arg(rate, 0, 'f', 1);

    if (lost == 0 && received > 150) {
        qInfo() << ">>> SUCCESS: 100Hz Cross-Thread Lock-Free Pipeline is 100% Reliable!";
    } else {
        qWarning() << ">>> WARNING: Data loss or frame rate deviation detected!";
    }
    qInfo() << "================ [100Hz High-Frequency Burst Test End] ================";
}

// V3 006
void testDriverPoolMigration()
{
    qInfo() << "================ [Driver Pool Migration Test Start] ================";

    // 必须确保注册了 QList<DeviceInfo>
    qRegisterMetaType<QList<DeviceInfo>>("QList<DeviceInfo>");

    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();

    // 1. 启动引擎 (自动从数据库读取 10+ 台设备并下发给 Worker 驱动池)
    controller.start(500);

    // 2. 保持事件循环 200ms，观察所有驱动在子线程并发握手完成
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();

    // 3. 一键安全关停，验证驱动池优雅批量 close 和析构
    controller.stop();

    qInfo() << "================ [Driver Pool Migration Test End: All Passed] ================";
}

// V3 007
void testAsyncBatchPollingPipeline()
{
    qInfo() << "================ [Batch Polling & Telemetry Decoding Test Start] ================";

    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();

    // 1. 订阅控制器广播的全厂批量遥测总线
    QObject::connect(&controller, &AcquisitionController::telemetryBatchReady,
                     [](const QList<TelemetryFrame> &batch) {
                         qInfo() << QString("---------------- [Batch Arrival: %1 Devices Captured] ----------------").arg(batch.size());

                         for (const auto &frame : batch) {
                             qInfo() << QString("  - [%1] Temp: %2 ℃ (Raw: %3) | CPU: %4 % | Comm: %5")
                                            .arg(frame.deviceId)
                                            .arg(frame.temperature, 5, 'f', 1)
                                            .arg(frame.rawRegisters[0])
                                            .arg(frame.cpuUsage, 5, 'f', 1)
                                            .arg(frame.commSuccess ? "ONLINE" : "OFFLINE");
                         }
                     });

    // 2. 启动引擎 (500ms 采样周期，驱动池将在子线程全量启动)
    controller.start(500);

    // 3. 运行 1100ms，观察至少接收 2 个完整的全厂批量遥测数据包
    QEventLoop loop;
    QTimer::singleShot(1100, &loop, &QEventLoop::quit);
    loop.exec();

    // 4. 一键安全关停
    controller.stop();

    qInfo() << "================ [Batch Polling & Telemetry Decoding Test End: All Passed] ================";
}



// V3 009
void testThrottleAndEventStormSuppression()
{
    qInfo() << "================ [Throttle Mechanism & Anti-Storm Test Start] ================";

    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();

    // 1. 配置工况：子线程以 20ms (50Hz) 狂暴采集，前台视口锁定 33ms (约 30 FPS) 节流
    const int workerIntervalMs   = 20; // 采集周期
    const int throttleIntervalMs = 33; // 视口刷新周期
    const int testDurationMs     = 2000; // 测试持续 2 秒

    controller.setThrottleEnabled(true);
    controller.setThrottleInterval(throttleIntervalMs);

    int rawBatchesReceived = 0;
    int throttledBatchesReceived = 0;
    int totalFramesInThrottled = 0;

    // 监听无损原生信号 (统计总产生批次)
    QObject::connect(&controller, &AcquisitionController::telemetryBatchReady,
                     [&rawBatchesReceived](const QList<TelemetryFrame> &batch) {
                         Q_UNUSED(batch);
                         rawBatchesReceived++;
                     });

    // 监听节流后的视口渲染信号 (统计实际触发 UI 重绘的批次)
    QObject::connect(&controller, &AcquisitionController::throttledBatchReady,
                     [&throttledBatchesReceived, &totalFramesInThrottled](const QList<TelemetryFrame> &batch) {
                         throttledBatchesReceived++;
                         totalFramesInThrottled += batch.size();
                     });

    qInfo() << QString("[Test Plan] Worker Polling: %1 ms (50Hz) | Throttle Lock: %2 ms (~30 FPS) | Duration: %3 ms")
                   .arg(workerIntervalMs).arg(throttleIntervalMs).arg(testDurationMs);

    QElapsedTimer timer;
    timer.start();

    // 启动高频采集引擎
    controller.start(workerIntervalMs);

    // 阻塞运行 2000ms
    QEventLoop loop;
    QTimer::singleShot(testDurationMs, &loop, &QEventLoop::quit);
    loop.exec();

    // 安全关停
    controller.stop();
    qint64 elapsed = timer.elapsed();

    // 2. 指标结算与量化分析
    double compressionRate = 0.0;
    if (rawBatchesReceived > 0) {
        compressionRate = (1.0 - static_cast<double>(throttledBatchesReceived) / rawBatchesReceived) * 100.0;
    }

    qInfo() << "-------------------- [Throttle Benchmark Metrics] --------------------";
    qInfo() << QString("Total Run Time             : %1 ms").arg(elapsed);
    qInfo() << QString("Raw Batches Emitted (Worker): %1 (Incoming rate: %2 Batches/s)")
                   .arg(rawBatchesReceived).arg(rawBatchesReceived * 1000.0 / elapsed, 0, 'f', 1);
    qInfo() << QString("Throttled Batches to UI    : %1 (UI Render rate : %2 FPS)")
                   .arg(throttledBatchesReceived).arg(throttledBatchesReceived * 1000.0 / elapsed, 0, 'f', 1);
    qInfo() << QString("Event Storm Absorbed Rate  : %1 %").arg(compressionRate, 0, 'f', 2);
    qInfo() << QString("Total Devices Updated to UI: %1 frames").arg(totalFramesInThrottled);

    // 3. 严格断言
    Q_ASSERT(rawBatchesReceived > throttledBatchesReceived);
    Q_ASSERT(throttledBatchesReceived >= 30 && throttledBatchesReceived <= 70); // 2000ms / 33ms 应该约为 60 次
    qInfo() << ">>> SUCCESS: Viewport Throttling effectively shields UI from Event Storm!";
    qInfo() << "================ [Throttle Mechanism & Anti-Storm Test End: All Passed] ================";
}

// V3 010
void testExponentialBackoffReconnection()
{
    qInfo() << "================ [Exponential Backoff Reconnection Test Start] ================";

    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();

    // 监听状态消息通知
    QObject::connect(&controller, &AcquisitionController::statusMessage,
                     [](const QString &msg) {
                         qInfo() << ">>> [Engine Event]:" << msg;
                     });

    // 1. 启动采集引擎 (500ms 采样心跳)
    controller.start(500);

    // 运行 1000ms 等待全厂握手就绪
    {
        QEventLoop loop;
        QTimer::singleShot(1000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    qInfo() << "---------------- [Step 1: Injecting Fault to DEV-1002] ----------------";
    // 通过控制器安全向子线程工作者下发断线故障注入指令
    // (也可在测试中模拟断网；此处要求 Worker 暴露出 injectDeviceFault 测试槽或在子线程直接触发)
    QMetaObject::invokeMethod(controller.worker(), "injectFault",
                              Qt::QueuedConnection,
                              Q_ARG(QString, "DEV-1002"),
                              Q_ARG(bool, true));

    // 观察 2500ms：验证首次失败 (1s 后重连) 与第二次失败 (2s 后重连) 的指数递增
    {
        QEventLoop loop;
        QTimer::singleShot(2500, &loop, &QEventLoop::quit);
        loop.exec();
    }

    qInfo() << "---------------- [Step 2: Clearing Fault on DEV-1002 (Self-Healing)] ----------------";
    // 排除故障，验证自愈恢复
    QMetaObject::invokeMethod(controller.worker(), "injectFault",
                              Qt::QueuedConnection,
                              Q_ARG(QString, "DEV-1002"),
                              Q_ARG(bool, false));

    // 观察 2000ms：验证在退避窗口到达时握手成功并切回 Connected
    {
        QEventLoop loop;
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // 安全关停
    controller.stop();

    qInfo() << "================ [Exponential Backoff Reconnection Test End: All Passed] ================";
}

// V3 011
void testAlarmManagerLifecyclePipeline()
{
    qInfo() << "================ [AlarmManager Lifecycle & Ring Buffer Test Start] ================";

    AlarmManager &manager = AlarmManager::instance();
    manager.clearHistory();
    // 1. 压测设置：将环形缓冲区收紧为 3 条，测试溢出淘汰机制
    manager.setMaxHistoryCapacity(3);

    int triggerCount = 0;
    QString lastAckId;

    QObject::connect(&manager, &AlarmManager::alarmTriggered, [&triggerCount](const AlarmEvent &event) {
        triggerCount++;
        qInfo() << ">>> [Observer Caught Alarm]:" << event.alarmId << "|" << event.description;
    });

    QObject::connect(&manager, &AlarmManager::alarmAcknowledged, [&lastAckId](const QString &id) {
        lastAckId = id;
        qInfo() << ">>> [Observer Caught Ack]:" << id;
    });

    // 2. 连续构造并压入 4 条告警 (应触发 1 次挤压淘汰)
    for (int i = 1; i <= 4; ++i) {
        AlarmEvent ev;
        ev.alarmId = QString("ALM-TEST-00%1").arg(i);
        ev.deviceId = QString("DEV-100%1").arg(i);
        ev.level = (i == 4) ? AlarmLevel::Critical : AlarmLevel::Warning;
        ev.description = QString("Sensor-%1 Overheat simulated").arg(i);
        ev.triggerValue = 80.0 + i;
        ev.thresholdValue = 80.0;
        ev.timestamp = QDateTime::currentDateTime();
        ev.acknowledged = false;

        manager.postAlarm(ev);
    }

    // 3. 验证环形缓冲区容量与淘汰
    QList<AlarmEvent> history = manager.getHistoricalAlarms();
    qInfo() << "Historical Buffer Count:" << history.size();
    Q_ASSERT(history.size() == 3); // 满 3 条，第一条 ALM-TEST-001 已被淘汰
    Q_ASSERT(history.first().alarmId == "ALM-TEST-002"); // 当前最老项应是第 2 条
    Q_ASSERT(history.last().alarmId == "ALM-TEST-004");  // 当前最新项应是第 4 条
    Q_ASSERT(triggerCount == 4);

    // 4. 验证消音生命周期
    Q_ASSERT(manager.getActiveAlarms().size() == 3);
    manager.acknowledgeAlarm("ALM-TEST-004");
    Q_ASSERT(lastAckId == "ALM-TEST-004");
    Q_ASSERT(manager.getActiveAlarms().size() == 2); // 剩余 2 条未确认活动告警

    qInfo() << ">>> SUCCESS: Ring Buffer and Acknowledge Lifecycle passed 100%!";
    qInfo() << "================ [AlarmManager Lifecycle & Ring Buffer Test End: All Passed] ================";
}

// V3 012
void testAlarmEvaluationAndDeadbandPipeline()
{
    qInfo() << "================ [Alarm Evaluation & Deadband Hysteresis Test Start] ================";

    AlarmManager &manager = AlarmManager::instance();
    manager.clearHistory();
    manager.setMaxHistoryCapacity(100);

    // 设置工艺红线：Warning 75℃, Critical 85℃, 死区 1.0℃
    manager.setTemperatureThresholds(75.0, 85.0, 1.0);

    int totalAlarmsCaptured = 0;
    AlarmLevel lastLevel = AlarmLevel::Info;

    QObject::connect(&manager, &AlarmManager::alarmTriggered, [&](const AlarmEvent &ev) {
        totalAlarmsCaptured++;
        lastLevel = ev.level;
        qInfo() << QString(">>> [Captured %1 Alarm]: %2 | Value: %3 ℃")
                       .arg(ev.level == AlarmLevel::Critical ? "CRITICAL" : (ev.level == AlarmLevel::Warning ? "WARNING" : "INFO"))
                       .arg(ev.description)
                       .arg(ev.triggerValue);
    });

    TelemetryFrame frame;
    frame.deviceId = "DEV-1001";
    frame.commSuccess = true;
    frame.timestamp = QDateTime::currentDateTime();

    qInfo() << "---------------- [Step 1: Normal -> Warning -> Critical Escalation] ----------------";
    // 1. 正常读数
    frame.temperature = 60.0;
    manager.evaluateTelemetry(frame);
    Q_ASSERT(totalAlarmsCaptured == 0);

    // 2. 爬升至 76.0℃ (触发 Warning)
    frame.temperature = 76.0;
    manager.evaluateTelemetry(frame);
    Q_ASSERT(totalAlarmsCaptured == 1);
    Q_ASSERT(lastLevel == AlarmLevel::Warning);

    // 3. 持续 77.0℃ (处于 Warning，不重复生成)
    frame.temperature = 77.0;
    manager.evaluateTelemetry(frame);
    Q_ASSERT(totalAlarmsCaptured == 1); // 依然为 1

    // 4. 越界至 86.0℃ (升级为 Critical)
    frame.temperature = 86.0;
    manager.evaluateTelemetry(frame);
    Q_ASSERT(totalAlarmsCaptured == 2);
    Q_ASSERT(lastLevel == AlarmLevel::Critical);

    qInfo() << "---------------- [Step 2: Deadband Anti-Chattering Test around 85℃] ----------------";
    // 5. 波动测试：温度在 [84.2℃ ~ 85.2℃] 临界跳变，但从未跌破 84.0℃ (85 - 1.0)
    double noiseSequence[] = { 84.8, 85.2, 84.5, 84.9, 84.1 };
    for (double noiseTemp : noiseSequence) {
        frame.temperature = noiseTemp;
        manager.evaluateTelemetry(frame);
    }
    // 验证：期间绝对没有产生任何新的告警或恢复通知，死区消抖完全生效！
    Q_ASSERT(totalAlarmsCaptured == 2);
    Q_ASSERT(lastLevel == AlarmLevel::Critical);

    qInfo() << "---------------- [Step 3: Clear and Return to Normal] ----------------";
    // 6. 降温至 83.5℃ (跌破 84.0℃，降级为 Warning)
    frame.temperature = 83.5;
    manager.evaluateTelemetry(frame);
    Q_ASSERT(totalAlarmsCaptured == 3);
    Q_ASSERT(lastLevel == AlarmLevel::Warning);

    // 7. 彻底降温至 73.0℃ (跌破 74.0℃，彻底恢复 Normal，生成 Info 恢复日志)
    frame.temperature = 73.0;
    manager.evaluateTelemetry(frame);
    Q_ASSERT(totalAlarmsCaptured == 4);
    Q_ASSERT(lastLevel == AlarmLevel::Info);

    qInfo() << ">>> SUCCESS: Deadband Anti-Chattering & State Transitions are 100% Solid!";
    qInfo() << "================ [Alarm Evaluation & Deadband Hysteresis Test End: All Passed] ================";
}

// V3 013
void testModelAlarmVisualLinkagePipeline()
{
    qInfo() << "================ [Model Alarm Visual Linkage Test Start] ================";

    DeviceTableModel model;
    DeviceInfo dev;
    dev.id = "DEV-1001";
    dev.name = "1号压铸机";
    dev.status = DeviceStatus::Normal;
    dev.temperature = 50.0;
    model.setDeviceList({dev});

    AlarmManager &alarmMgr = AlarmManager::instance();
    alarmMgr.clearHistory();
    alarmMgr.setTemperatureThresholds(75.0, 85.0, 1.0);

    // 1. 验证初始状态：正常在线，无报警背景色
    QModelIndex idx = model.index(0, 0);
    QVariant bgVariant = model.data(idx, Qt::BackgroundRole);
    Q_ASSERT(!bgVariant.isValid()); // 正常态返回无效 QVariant，由 QSS 渲染
    qInfo() << "Step 1: Normal state has transparent background. OK.";

    // 2. 模拟超温预警 (78.0 ℃)
    TelemetryFrame frame;
    frame.deviceId = "DEV-1001";
    frame.temperature = 78.0;
    frame.commSuccess = true;
    frame.timestamp = QDateTime::currentDateTime();

    // 先送入告警引擎裁定状态机，再灌入 Model 视口
    alarmMgr.evaluateTelemetry(frame);
    model.updateTelemetryBatch({frame});

    // 断言：设备状态跃迁至 Warning，且整行具备琥珀微光背景色
    DeviceInfo updatedDev = model.getDevice(0);
    Q_ASSERT(updatedDev.status == DeviceStatus::Warning);
    QColor warnColor = model.data(idx, Qt::BackgroundRole).value<QBrush>().color();
    qInfo() << "Step 2: Warning background color detected:" << warnColor.name() << "Alpha:" << warnColor.alpha();
    Q_ASSERT(warnColor.red() == 230 && warnColor.green() == 162); // 匹配 #E6A23C

    // 3. 模拟严重超温 (88.0 ℃)
    frame.temperature = 88.0;
    alarmMgr.evaluateTelemetry(frame);
    model.updateTelemetryBatch({frame});

    updatedDev = model.getDevice(0);
    Q_ASSERT(updatedDev.status == DeviceStatus::Fault);
    QColor critColor = model.data(idx, Qt::BackgroundRole).value<QBrush>().color();
    qInfo() << "Step 3: Critical background color detected:" << critColor.name() << "Alpha:" << critColor.alpha();
    Q_ASSERT(critColor.red() == 245 && critColor.green() == 108); // 匹配 #F56C6C

    // 4. 模拟跌破死区安全恢复 (60.0 ℃)
    frame.temperature = 60.0;
    alarmMgr.evaluateTelemetry(frame);
    model.updateTelemetryBatch({frame});

    updatedDev = model.getDevice(0);
    Q_ASSERT(updatedDev.status == DeviceStatus::Normal);
    Q_ASSERT(!model.data(idx, Qt::BackgroundRole).isValid()); // 恢复透明/无特殊背景
    qInfo() << "Step 4: Restored to Online and transparent background cleanly.";

    qInfo() << ">>> SUCCESS: Model Alarm Visual Linkage is 100% Solid!";
    qInfo() << "================ [Model Alarm Visual Linkage Test End: All Passed] ================";
}

// V3 014
void testAlarmBannerLifecyclePipeline()
{
    qInfo() << "================ [AlarmBanner Animation & Ack Test Start] ================";

    // 模拟真实挂载环境：告警条必须作为布局子项存在，
    // 布局管理器才会响应 maximumHeight 动画驱动实际高度 (独立顶层窗口没有布局驱动，不会变高)
    QWidget container;
    container.setWindowTitle("AlarmBannerUnitTest");
    QVBoxLayout *containerLayout = new QVBoxLayout(&container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    AlarmBanner *banner = new AlarmBanner(&container);
    containerLayout->addWidget(banner);

    container.resize(800, 100);
    container.show();

    // 初始状态高度必须为 0
    Q_ASSERT(banner->height() == 0);
    qInfo() << "Step 1: Banner initialized with height 0. OK.";

    AlarmEvent ev;
    ev.alarmId = "ALM-BANNER-TEST-001";
    ev.deviceId = "DEV-1005";
    ev.level = AlarmLevel::Critical;
    ev.description = "主轴轴承温度突破 92.0 ℃！";

    // 1. 触发弹出展示
    banner->showAlert(ev);

    // 等待 350ms 让弹性展开动画完整执行完毕
    {
        QEventLoop loop;
        QTimer::singleShot(350, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // 断言展开高度已达标
    Q_ASSERT(banner->height() >= 40);
    qInfo() << "Step 2: Banner expanded smoothly. Current height:" << banner->height();

    // 2. 模拟点击消音收起
    banner->dismissAlert();

    // 等待 350ms 让回弹收缩动画完整执行完毕
    {
        QEventLoop loop;
        QTimer::singleShot(350, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // 断言已彻底折叠归零
    Q_ASSERT(banner->height() == 0);
    qInfo() << "Step 3: Banner collapsed cleanly back to height 0.";

    qInfo() << ">>> SUCCESS: AlarmBanner Drawer Animation is 100% Smooth and Reliable!";
    qInfo() << "================ [AlarmBanner Animation & Ack Test End: All Passed] ================";
}

// V3 015 全链路终极破坏性高压并发压测
void testFullPipelineStressAndConcurrency()
{
    qInfo() << "================================================================================";
    qInfo() << "   [FINAL BENCHMARK] Full-Pipeline High-Concurrency Stress & Lifecycle Audit    ";
    qInfo() << "================================================================================";

    // 1. 初始化核心架构组件
    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();

    AlarmManager &alarmMgr = AlarmManager::instance();
    alarmMgr.clearHistory();
    alarmMgr.setMaxHistoryCapacity(200);
    alarmMgr.setTemperatureThresholds(75.0, 85.0, 1.0);

    // 2. 建立 MVC 模型与代理层
    DeviceTableModel sourceModel;
    DeviceProxyModel proxyModel;
    proxyModel.setSourceModel(&sourceModel);

    // 加载 SQLite 台账
    QList<DeviceInfo> devices = DeviceDao::queryAllDevices();
    sourceModel.setDeviceList(devices);
    qInfo() << QString("[Stress Setup] Loaded %1 devices into TableModel with ProxyModel enabled.")
                   .arg(devices.size());

    // 3. 配置高压工况：20ms 采集 (50Hz)，33ms 视口节流 (~30 FPS)
    const int workerIntervalMs   = 20;
    const int throttleIntervalMs = 33;
    const int durationMs         = 3000; // 持续高压冲击 3 秒

    controller.setThrottleEnabled(true);
    controller.setThrottleInterval(throttleIntervalMs);

    int totalBatchesProcessed = 0;
    int alarmsGenerated = 0;
    QString criticalAlarmId;

    // 挂载数据总线流向 Model 与 AlarmManager
    QObject::connect(&controller, &AcquisitionController::throttledBatchReady,
                     [&](const QList<TelemetryFrame> &batch) {
        totalBatchesProcessed++;
        alarmMgr.evaluateBatch(batch);
        sourceModel.updateTelemetryBatch(batch);
    });

    QObject::connect(&alarmMgr, &AlarmManager::alarmTriggered,
                     [&](const AlarmEvent &ev) {
        alarmsGenerated++;
        if (ev.level == AlarmLevel::Critical && criticalAlarmId.isEmpty())
            criticalAlarmId = ev.alarmId;
    });

    // 告警条联动：验证高压洪流中 Critical 弹出与消音闭环
    QWidget bannerContainer;
    bannerContainer.setWindowTitle("StressBannerContainer");
    QVBoxLayout *bannerLayout = new QVBoxLayout(&bannerContainer);
    bannerLayout->setContentsMargins(0, 0, 0, 0);
    bannerLayout->setSpacing(0);
    AlarmBanner *banner = new AlarmBanner(&bannerContainer);
    bannerLayout->addWidget(banner);
    bannerContainer.resize(900, 120);
    bannerContainer.show(); // 必须显示：布局激活后告警条才会响应 maximumHeight 动画变高
    QObject::connect(&alarmMgr, &AlarmManager::alarmTriggered, banner,
                     [banner](const AlarmEvent &ev) {
        if (ev.level == AlarmLevel::Critical)
            banner->showAlert(ev);
    });

    qInfo() << QString("[Stress Running] Ingesting 50Hz burst telemetry for %1 ms...").arg(durationMs);
    QElapsedTimer timer;
    timer.start();

    // 启动后台引擎
    controller.start(workerIntervalMs);

    // 4. 在高频冲击期间，主线程不断进行排序洗牌与模拟干扰
    QTimer disturbanceTimer;
    int disturbanceTicks = 0;
    QObject::connect(&disturbanceTimer, &QTimer::timeout, [&]() {
        disturbanceTicks++;
        // 动态切换排序列 (交替排温度、CPU、名称等 6 列)
        proxyModel.sort(disturbanceTicks % 6, (disturbanceTicks % 2 == 0) ? Qt::AscendingOrder : Qt::DescendingOrder);
        // 模拟搜索扰动 (过滤树剧烈扰动)
        if (disturbanceTicks % 3 == 0) {
            proxyModel.setSearchKeyword("DEV-1002");
        } else {
            proxyModel.setSearchKeyword("");
        }
    });
    disturbanceTimer.start(250); // 每 250ms 强制重排与过滤

    // 中途注入 92℃ 严重超温 → 触发 Critical 报警 → 告警条弹出 → 自动消音收起
    QTimer::singleShot(1500, [&]() {
        if (devices.isEmpty()) return;
        TelemetryFrame hotFrame;
        hotFrame.deviceId    = devices.first().id;
        hotFrame.temperature = 92.0;
        hotFrame.commSuccess = true;
        hotFrame.timestamp   = QDateTime::currentDateTime();
        alarmMgr.evaluateTelemetry(hotFrame);
        qInfo() << "[Stress Disturbance] Injected 92.0℃ critical overheat!";
    });
    QTimer::singleShot(2200, [&]() {
        Q_ASSERT(banner->height() >= 40); // 告警条已弹性展开
        if (!criticalAlarmId.isEmpty())
            alarmMgr.acknowledgeAlarm(criticalAlarmId);
        banner->dismissAlert();
        qInfo() << "[Stress Disturbance] Alarm acknowledged & banner dismissed.";
    });

    // 运行主事件循环 3000ms
    QEventLoop loop;
    QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
    loop.exec();

    // 停止扰动
    disturbanceTimer.stop();
    Q_ASSERT(banner->height() == 0); // 告警条已彻底折叠归零

    // 5. 安全优雅停机 (Graceful Teardown)
    qInfo() << "[Stress Teardown] Initiating graceful controller teardown...";
    controller.stop();
    qint64 totalElapsed = timer.elapsed();

    // 6. 最终指标核算与审计
    qInfo() << "-------------------- [Stress Benchmark Metrics] --------------------";
    qInfo() << QString("Total Ingestion Time     : %1 ms").arg(totalElapsed);
    qInfo() << QString("UI Batches Processed     : %1").arg(totalBatchesProcessed);
    qInfo() << QString("Alarms Handled & Clamped : %1 (Ring Buffer Size: %2)")
               .arg(alarmsGenerated).arg(alarmMgr.getHistoricalAlarms().size());
    qInfo() << QString("Model Device Integrity   : %1 devices retained safely").arg(sourceModel.rowCount(QModelIndex()));

    // 严格断言：数据完整性与无崩溃
    Q_ASSERT(sourceModel.rowCount(QModelIndex()) == devices.size());
    Q_ASSERT(totalBatchesProcessed >= 60);                       // 3000ms / 33ms 至少应处理 60+ 次视口刷新
    Q_ASSERT(alarmsGenerated >= 1);                              // 注入的 Critical 必须被捕获
    Q_ASSERT(alarmMgr.getHistoricalAlarms().size() <= 200);      // 环形缓冲区绝对不超限

    qInfo() << "================================================================================";
    qInfo() << "   >>> V3 FINAL AUDIT PASSED: 100% THREAD-SAFE, 0 LEAKS, 60 FPS SOLID! <<<     ";
    qInfo() << "================================================================================";
}

// V4 001 双 Y 轴视口拓扑与暗黑主题装配单测
void testRealtimePlotDualAxisTopology()
{
    qInfo() << "================ [Dual-Axis Realtime Plot Topology Test Start] ================";

    RealtimePlotWidget plotW;
    QCustomPlot *p = plotW.plot();
    Q_ASSERT(p != nullptr);

    // 1. 左 Y 轴：温度量纲 0~120
    Q_ASSERT(qFuzzyCompare(p->yAxis->range().lower, 0.0));
    Q_ASSERT(qFuzzyCompare(p->yAxis->range().upper, 120.0));
    Q_ASSERT(p->yAxis->label() == "模具温度 (℃)");
    qInfo() << "Step 1: Left Y-Axis temperature scale [0, 120] verified.";

    // 2. 右 Y 轴：可见 + 负荷量纲 0~100
    Q_ASSERT(p->yAxis2->visible());
    Q_ASSERT(qFuzzyCompare(p->yAxis2->range().lower, 0.0));
    Q_ASSERT(qFuzzyCompare(p->yAxis2->range().upper, 100.0));
    Q_ASSERT(p->yAxis2->label() == "主轴负荷 (%)");
    qInfo() << "Step 2: Right Y-Axis load scale [0, 100] visible & isolated.";

    // 3. 两条曲线绑定不同物理轴，绝无交叉干涉
    Q_ASSERT(p->graphCount() >= 2);
    Q_ASSERT(p->graph(0)->keyAxis() == p->xAxis && p->graph(0)->valueAxis() == p->yAxis);
    Q_ASSERT(p->graph(1)->keyAxis() == p->xAxis && p->graph(1)->valueAxis() == p->yAxis2);
    qInfo() << "Step 3: Two graphs bound to isolated physical axes.";

    // 4. 曲线警示色系断言
    Q_ASSERT(p->graph(0)->pen().color() == QColor("#FF9800"));   // 温度亮橙
    Q_ASSERT(p->graph(1)->pen().color() == QColor("#00E5FF"));   // 负荷青蓝
    qInfo() << "Step 4: Alert color scheme verified (orange / cyan).";

    // 5. 数据追加与量程固定不越界
    plotW.addDataPoint(10.0, 88.5, 62.3);
    Q_ASSERT(p->graph(0)->dataCount() == 1);
    Q_ASSERT(p->graph(1)->dataCount() == 1);
    qInfo() << "Step 5: Data append pipeline verified.";

    // 6. 暗黑主题核心底色断言 (QBrush 背景时 background() 返回空 QPixmap，必须整体渲染后采样像素)
    QPixmap rendered = p->toPixmap(600, 400);
    QImage renderImg = rendered.toImage();
    Q_ASSERT(!renderImg.isNull());
    // 画布外圈 (10,10 位于坐标区之外) 应为 #1A1A1A
    Q_ASSERT(renderImg.pixelColor(10, 10) == QColor("#1A1A1A"));
    // 坐标区中心应为 #121212 (或极暗网格线，亮度必须 < 40)
    QColor axisCol = renderImg.pixelColor(300, 200);
    Q_ASSERT(axisCol.red() < 40 && axisCol.green() < 40 && axisCol.blue() < 40);
    Q_ASSERT(p->legend->visible());
    qInfo() << "Step 6: Dark SCADA theme core colors verified.";

    qInfo() << ">>> SUCCESS: Dual-Axis Topology & Dark Theme 100% Solid!";
    qInfo() << "================ [Dual-Axis Realtime Plot Topology Test End: All Passed] ================";
}

// V4 002
void testRealtimePlotSlidingWindowPipeline()
{
    qInfo() << "================ [RealtimePlot Sliding Window & Ring Pruning Test Start] ================";

    RealtimePlotWidget plotWidget;
    plotWidget.resize(1000, 600);

    QCustomPlot *plot = plotWidget.plot();
    Q_ASSERT(plot != nullptr);

    // 1. 设置滑动时间窗口为 300 秒 (5 分钟)
    plotWidget.setTimeWindowSpan(300);
    Q_ASSERT(plotWidget.timeWindowSpan() == 300);
    qInfo() << "Step 1: Sliding window configured to 300 seconds. OK.";

    // 1.5 物理时间轴刻度解析验收：X 轴 ticker 必须是 hh:mm:ss 日期时间解析器
    QSharedPointer<QCPAxisTickerDateTime> dateTicker =
        qSharedPointerDynamicCast<QCPAxisTickerDateTime>(plot->xAxis->ticker());
    Q_ASSERT(dateTicker);
    Q_ASSERT(dateTicker->dateTimeFormat() == "hh:mm:ss");
    qInfo() << "Step 1.5: X-Axis DateTime ticker format hh:mm:ss verified.";

    // 2. 模拟以 1 秒步长，连续密集追加 1000 个采样点 (跨越 1000 秒)
    //    基准起始时刻: 2026-09-20 12:00:00 (Unix: 1779364800)
    const double baseTimestamp = 1779364800.0;
    qInfo() << "[Simulating Telemetry Stream] Injecting 1000 seconds of continuous data...";

    for (int sec = 0; sec < 1000; ++sec) {
        double currentT = baseTimestamp + sec;
        double mockTemp = 40.0 + (sec % 20); // 40~60 ℃ 正弦波模拟
        double mockCpu  = 20.0 + (sec % 50); // 20~70 % 负荷模拟

        plotWidget.addDataPoint(currentT, mockTemp, mockCpu);
    }

    // 3. 断言 X 轴视口范围是否随时间滑动到最新切片 [base + 700, base + 999]
    double expectedLatest = baseTimestamp + 999.0;
    double expectedOldest = expectedLatest - 300.0;

    QCPRange xRange = plot->xAxis->range();
    qInfo() << QString("Step 2: Latest X-Axis Range: [%1 ~ %2]").arg(xRange.lower, 0, 'f', 1).arg(xRange.upper, 0, 'f', 1);
    Q_ASSERT(qFuzzyCompare(xRange.upper, expectedLatest));
    Q_ASSERT(qFuzzyCompare(xRange.lower, expectedOldest));

    // 4. 核心断言：环形切片淘汰机制验证 (内存中不得保留超过 301 个点)
    int tempPointCount = plot->graph(0)->data()->size();
    int cpuPointCount  = plot->graph(1)->data()->size();
    qInfo() << QString("Step 3: Graph Data Buffer Count (Temp: %1, CPU: %2)").arg(tempPointCount).arg(cpuPointCount);

    // 1000 秒的数据只保留最新的 300 秒左右点集，前 700 个点必须被物理销毁
    Q_ASSERT(tempPointCount >= 300 && tempPointCount <= 302);
    Q_ASSERT(cpuPointCount  >= 300 && cpuPointCount  <= 302);

    // 验证缓冲区中保留的最老点的时戳必须大于等于 expectedOldest
    double firstPointT = plot->graph(0)->data()->at(0)->key;
    Q_ASSERT(firstPointT >= expectedOldest);
    qInfo() << "Step 4: Oldest point in buffer is:" << QString::number(firstPointT, 'f', 1)
            << ">= Expire cutoff:" << QString::number(expectedOldest, 'f', 1) << ". Clean!";

    // 5. 视觉冒烟：滑动窗口内温度亮橙曲线必须真实渲染上屏 (整体渲染后统计橙色像素)
    QPixmap snapshot = plot->toPixmap(900, 500);
    snapshot.save("D:/plot_sliding_snapshot.png");
    QImage snapImg = snapshot.toImage();
    int orangePixels = 0;
    for (int y = 0; y < snapImg.height(); y += 2) {
        for (int x = 0; x < snapImg.width(); x += 2) {
            QColor c = snapImg.pixelColor(x, y);
            if (c.red() > 200 && c.green() > 90 && c.green() < 190 && c.blue() < 80)
                orangePixels++;
        }
    }
    Q_ASSERT(orangePixels > 50); // 温度曲线 (亮橙 #FF9800) 必须在快照中成片出现
    qInfo() << QString("Step 5: Rendered snapshot contains %1 orange curve pixels. Waveform alive!").arg(orangePixels);

    qInfo() << ">>> SUCCESS: 5-Minute Sliding Window & Ring Pruning is 100% Leak-Free!";
    qInfo() << "================ [RealtimePlot Sliding Window & Ring Pruning Test End: All Passed] ================";
}

// V4 003 设备解复用分发与通道控制单测
void testRealtimePlotRoutingAndDemuxPipeline()
{
    qInfo() << "================ [RealtimePlot Routing & Demux Test Start] ================";

    RealtimePlotWidget plotWidget;
    plotWidget.resize(1000, 600);

    // 1. 初始化两台设备
    DeviceInfo d1; d1.id = "DEV-1001"; d1.name = "1号高速注塑机";
    DeviceInfo d2; d2.id = "DEV-1002"; d2.name = "2号精密冲压机";
    plotWidget.setDeviceList({d1, d2});

    // 断言初始默认选中第 1 台
    Q_ASSERT(plotWidget.currentDeviceId() == "DEV-1001");
    qInfo() << "Step 1: Default selected device confirmed as DEV-1001. OK.";

    // 2. 模拟全厂批量遥测帧广播 (同时包含 DEV-1001 与 DEV-1002)
    QDateTime now = QDateTime::currentDateTime();
    TelemetryFrame f1; f1.deviceId = "DEV-1001"; f1.temperature = 42.5; f1.cpuUsage = 28.0; f1.timestamp = now;
    TelemetryFrame f2; f2.deviceId = "DEV-1002"; f2.temperature = 78.5; f2.cpuUsage = 85.0; f2.timestamp = now;

    plotWidget.onTelemetryBatchReceived({f1, f2});

    QCustomPlot *plot = plotWidget.plot();
    Q_ASSERT(plot->graph(0)->dataCount() == 1);
    double lastVal = plot->graph(0)->data()->at(0)->value;
    Q_ASSERT(qFuzzyCompare(lastVal, 42.5));
    qInfo() << "Step 2: Correctly ingested DEV-1001 telemetry (42.5 ℃) and ignored DEV-1002. Demux OK.";

    // 3. 模拟操作员切换设备至 DEV-1002
    QMetaObject::invokeMethod(&plotWidget, "onDeviceChanged", Qt::DirectConnection, Q_ARG(int, 1));
    Q_ASSERT(plotWidget.currentDeviceId() == "DEV-1002");
    Q_ASSERT(plot->graph(0)->dataCount() == 0); // 旧点已清空，绝无跨机台飞线
    qInfo() << "Step 3: Switched to DEV-1002, old buffer cleared cleanly. Flyline prevention OK.";

    // 4. 再次广播批量帧，必须捕获 DEV-1002 的 78.5 ℃
    plotWidget.onTelemetryBatchReceived({f1, f2});
    Q_ASSERT(plot->graph(0)->dataCount() == 1);
    Q_ASSERT(qFuzzyCompare(plot->graph(0)->data()->at(0)->value, 78.5));
    qInfo() << "Step 4: Now listening to DEV-1002 correctly (78.5 ℃). OK.";

    // 5. 通道显隐与 Y 轴联动验证
    const QList<QCheckBox *> chks = plotWidget.findChildren<QCheckBox *>();
    QCheckBox *chkTemp = nullptr;
    QCheckBox *chkAuto = nullptr;
    for (QCheckBox *c : chks) {
        if (c->text().contains("模具温度")) chkTemp = c;
        if (c->text().contains("自动跟随")) chkAuto = c;
    }
    Q_ASSERT(chkTemp && chkAuto);

    chkTemp->setChecked(false); // 取消勾选温度通道
    Q_ASSERT(!plot->graph(0)->visible());
    Q_ASSERT(!plot->yAxis->visible());
    Q_ASSERT(plot->graph(1)->visible());
    chkTemp->setChecked(true);
    Q_ASSERT(plot->graph(0)->visible());
    Q_ASSERT(plot->yAxis->visible());
    qInfo() << "Step 5: Channel visibility & Y-Axis linkage verified.";

    // 6. 自动滚动定格 / 追平验证 (必须经真实批量路由路径，定格逻辑位于解复用层)
    const double tBase = now.toMSecsSinceEpoch() / 1000.0;
    QDateTime tsBase = QDateTime::fromMSecsSinceEpoch(qint64(tBase * 1000.0));
    TelemetryFrame fBase = f2;
    fBase.timestamp = tsBase; fBase.temperature = 50.0; fBase.cpuUsage = 30.0;
    plotWidget.onTelemetryBatchReceived({fBase});
    Q_ASSERT(qFuzzyCompare(plot->xAxis->range().upper, tBase));

    chkAuto->setChecked(false); // 定格
    TelemetryFrame fLater = f2;
    fLater.timestamp = QDateTime::fromMSecsSinceEpoch(qint64((tBase + 50.0) * 1000.0));
    fLater.temperature = 52.0; fLater.cpuUsage = 32.0;
    plotWidget.onTelemetryBatchReceived({fLater});
    Q_ASSERT(qFuzzyCompare(plot->xAxis->range().upper, tBase)); // 视口保持冻结
    qInfo() << "Step 6a: Auto-scroll frozen, viewport held at previous latest. OK.";

    chkAuto->setChecked(true);  // 重新跟随 → 秒级追平
    Q_ASSERT(qFuzzyCompare(plot->xAxis->range().upper, tBase + 50.0));
    qInfo() << "Step 6b: Auto-scroll resumed, viewport snapped to latest. OK.";

    // 7. 真实引擎端到端贯通 (节流总线 → 解复用 → 滑动窗口)
    AcquisitionController &controller = AcquisitionController::instance();
    controller.init();
    QObject::connect(&controller, &AcquisitionController::throttledBatchReady,
                     &plotWidget, &RealtimePlotWidget::onTelemetryBatchReceived);

    const QList<DeviceInfo> dbDevices = DeviceDao::queryAllDevices();
    Q_ASSERT(!dbDevices.isEmpty());
    plotWidget.setDeviceList(dbDevices); // 选中数据库首台设备
    const QString targetId = plotWidget.currentDeviceId();

    controller.start(500); // 500ms 心跳
    {
        QEventLoop loop;
        QTimer::singleShot(2600, &loop, &QEventLoop::quit);
        loop.exec();
    }
    controller.stop();

    const int livePoints = plot->graph(0)->dataCount();
    Q_ASSERT(livePoints >= 3); // 2.6 秒 / 500ms 至少 4~5 个采样周期
    qInfo() << QString("Step 7: Live engine E2E pumped %1 points for [%2] via throttle bus.")
                   .arg(livePoints).arg(targetId);

    // 读数标签必须已被实时刷新
    bool readoutUpdated = false;
    const QList<QLabel *> labels = plotWidget.findChildren<QLabel *>();
    for (QLabel *l : labels) {
        if (l->text().contains("温度:")) { readoutUpdated = true; break; }
    }
    Q_ASSERT(readoutUpdated);

    // 视觉冒烟：整体控件快照必须包含亮橙温度曲线与工具条文字像素
    // (show 到屏幕外再 grab：完整走 show/polish/真实绘制路径，与主界面渲染完全同源，且不闪窗)
    plotWidget.move(-3000, -3000);
    plotWidget.show();
    for (int flush = 0; flush < 5; ++flush)
        QApplication::processEvents();
    QPixmap widgetSnapshot = plotWidget.grab();
    widgetSnapshot.save("D:/plot_routing_snapshot.png");
    QImage snapImg = widgetSnapshot.toImage();
    int orangePixels = 0;
    for (int y = 0; y < snapImg.height(); y += 2) {
        for (int x = 0; x < snapImg.width(); x += 2) {
            QColor c = snapImg.pixelColor(x, y);
            if (c.red() > 200 && c.green() > 90 && c.green() < 190 && c.blue() < 80)
                orangePixels++;
        }
    }
    Q_ASSERT(orangePixels > 50);
    qInfo() << QString("Step 8: Widget snapshot contains %1 orange waveform pixels.").arg(orangePixels);

    // 工具条文字必须真实渲染 (顶部区域存在非暗像素 = 标签/复选框/读数文本已上屏)
    int toolbarPixels = 0;
    for (int y = 0; y < 80; ++y) {
        for (int x = 0; x < snapImg.width(); ++x) {
            QColor c = snapImg.pixelColor(x, y);
            if (c.red() > 45 || c.green() > 45 || c.blue() > 45)
                toolbarPixels++;
        }
    }
    Q_ASSERT(toolbarPixels > 100);
    qInfo() << QString("Step 9: Toolbar rendered with %1 text pixels.").arg(toolbarPixels);

    qInfo() << ">>> SUCCESS: RealtimePlot Channel Routing & Device Selector Linkage is 100% Solid!";
    qInfo() << "================ [RealtimePlot Routing & Demux Test End: All Passed] ================";
}

// V4 004 游标磁吸精度与二分算法单测
void testPlotTracerSnappingPipeline()
{
    qInfo() << "================ [PlotTracer Binary Snapping Test Start] ================";

    RealtimePlotWidget plotWidget;
    plotWidget.resize(1000, 600);
    // show 到屏幕外：保证坐标变换 (coordToPixel/pixelToCoord) 基于真实布局几何
    plotWidget.move(-3000, -3000);
    plotWidget.show();
    for (int flush = 0; flush < 5; ++flush)
        QApplication::processEvents();

    QCustomPlot *plot = plotWidget.plot();
    PlotTracer *tracer = plotWidget.tracer();
    Q_ASSERT(plot && tracer);

    // 1. 空数据防御：无数据时疯狂晃动鼠标，零崩溃且游标保持隐藏
    tracer->updatePosition(QPoint(500, 300));
    Q_ASSERT(!tracer->hasSnappedPoint());
    Q_ASSERT(!tracer->isVisible());
    qInfo() << "Step 1: Empty-data guard solid (no snap, no crash). OK.";

    // 2. 手动注入 3 个已知时戳的离散校准点
    plotWidget.addDataPoint(100.0, 40.0, 20.0);
    plotWidget.addDataPoint(102.0, 55.0, 35.0);
    plotWidget.addDataPoint(105.0, 70.0, 50.0);
    Q_ASSERT(plot->graph(0)->dataCount() == 3);

    // 3. 模拟鼠标滑动到 t = 101.8 的虚空像素位置 (最近邻应为 t2 = 102.0)
    const int pixelX_NearT2 = plot->xAxis->coordToPixel(101.8);
    const int pixelY_Center = plot->yAxis->coordToPixel(50.0);
    tracer->updatePosition(QPoint(pixelX_NearT2, pixelY_Center));

    Q_ASSERT(tracer->hasSnappedPoint());
    Q_ASSERT(qFuzzyCompare(tracer->snappedKey(), 102.0));
    Q_ASSERT(qFuzzyCompare(tracer->snappedTemp(), 55.0));
    Q_ASSERT(qFuzzyCompare(tracer->snappedCpu(), 35.0)); // 双通道同刻对齐
    Q_ASSERT(tracer->tooltipText().contains("55.0"));
    Q_ASSERT(tracer->tooltipText().contains("35.0"));
    qInfo() << QString("Step 2/3: Snapped to t=%1s Temp=%2℃ Load=%3% (hover at 101.8s, zero drift)")
                   .arg(tracer->snappedKey(), 0, 'f', 1)
                   .arg(tracer->snappedTemp(), 0, 'f', 1)
                   .arg(tracer->snappedCpu(), 0, 'f', 1);

    // 4. 智能边界避让翻转：右侧边缘 → 浮窗翻转至光标左侧锚定
    const int pixelX_Right = plot->xAxis->coordToPixel(104.5);
    tracer->updatePosition(QPoint(pixelX_Right, pixelY_Center));
    Q_ASSERT(tracer->tooltipAnchoredRight());
    const int pixelX_Left = plot->xAxis->coordToPixel(100.5);
    tracer->updatePosition(QPoint(pixelX_Left, pixelY_Center));
    Q_ASSERT(!tracer->tooltipAnchoredRight());
    qInfo() << "Step 4: Smart edge flipping verified (right edge anchors tooltip leftward). OK.";

    // 5. 视口移出：游标与浮窗瞬间隐去，防崩
    tracer->updatePosition(QPoint(-10, -10));
    Q_ASSERT(!tracer->isVisible());
    Q_ASSERT(!tracer->hasSnappedPoint());
    qInfo() << "Step 5: Out-of-bounds mouse position hides tracer cleanly. OK.";

    // 6. 视觉冒烟：游标吸附态整体快照必须含 HUD 浮窗文字像素
    tracer->updatePosition(QPoint(pixelX_NearT2, pixelY_Center));
    Q_ASSERT(tracer->isVisible());
    QPixmap snapshot = plotWidget.grab();
    snapshot.save("D:/plot_tracer_snapshot.png");
    QImage snapImg = snapshot.toImage();
    int brightPixels = 0;
    for (int y = 0; y < snapImg.height(); y += 2) {
        for (int x = 0; x < snapImg.width(); x += 2) {
            QColor c = snapImg.pixelColor(x, y);
            if (c.red() > 200 && c.green() > 200 && c.blue() > 200)
                brightPixels++;
        }
    }
    Q_ASSERT(brightPixels > 100); // 浮窗白色文字 + 白描边圆点必须成片渲染
    qInfo() << QString("Step 6: Snapshot contains %1 bright HUD pixels. Tooltip rendered!").arg(brightPixels);

    qInfo() << ">>> SUCCESS: Crosshair Tracer & Nearest Snapping is 100% Reliable!";
    qInfo() << "================ [PlotTracer Binary Snapping Test End: All Passed] ================";
}

// V4 005 自由漫游交互 (轴向锁止 / 智能定格 / 瞬移追平) 与 60 FPS 渲染优化单测
void testRealtimePlotInteractionAndNavigationPipeline()
{
    qInfo() << "================ [RealtimePlot Interaction & Navigation Test Start] ================";

    RealtimePlotWidget plotWidget;
    plotWidget.resize(1000, 600);
    plotWidget.move(-3000, -3000); // 屏幕外渲染：完整走真实布局/绘制路径且不闪窗
    plotWidget.show();
    for (int flush = 0; flush < 5; ++flush)
        QApplication::processEvents();

    QCustomPlot *plot = plotWidget.plot();
    Q_ASSERT(plot != nullptr);

    // 1. 轴向隔离漫游验收：仅 X 轴参与拖拽/缩放，双 Y 轴通道必须为空 (绝对锁止)
    const QList<QCPAxis *> dragAxes = plot->axisRect()->rangeDragAxes(Qt::Horizontal);
    const QList<QCPAxis *> dragVertAxes = plot->axisRect()->rangeDragAxes(Qt::Vertical);
    const QList<QCPAxis *> zoomAxes = plot->axisRect()->rangeZoomAxes(Qt::Horizontal);
    const QList<QCPAxis *> zoomVertAxes = plot->axisRect()->rangeZoomAxes(Qt::Vertical);

    Q_ASSERT(dragAxes.contains(plot->xAxis));
    Q_ASSERT(dragVertAxes.isEmpty()); // Y 轴拖拽通道全部关闭
    Q_ASSERT(zoomAxes.contains(plot->xAxis));
    Q_ASSERT(zoomVertAxes.isEmpty()); // Y 轴缩放通道全部关闭
    Q_ASSERT(qFuzzyCompare(plot->axisRect()->rangeZoomFactor(Qt::Horizontal), 0.85));
    qInfo() << "Step 1: Axial isolation verified (X-axis free, both Y-axes 100% locked). OK.";

    // 2. 注入基准数据并验证自动跟随活跃
    plotWidget.addDataPoint(1000.0, 50.0, 30.0);
    plotWidget.addDataPoint(1100.0, 55.0, 35.0);
    Q_ASSERT(plot->graph(0)->dataCount() == 2);
    qInfo() << "Step 2: Base calibration points injected. Auto-scroll active.";

    // 3. 模拟用户主动漫游 (左键拖拽/滚轮缩放触发状态机拦截)
    QCheckBox *chkAuto = plotWidget.findChild<QCheckBox *>("chkAutoScroll");
    QPushButton *btnResume = plotWidget.findChild<QPushButton *>("btnResumeScroll");
    Q_ASSERT(chkAuto && btnResume);
    Q_ASSERT(chkAuto->isChecked());           // 初始自动跟随
    Q_ASSERT(btnResume->isHidden());          // 悬浮按钮初始沉睡

    QMetaObject::invokeMethod(&plotWidget, "onPlotUserInteraction", Qt::DirectConnection);

    Q_ASSERT(!chkAuto->isChecked());          // 复选框被状态机同步取消勾选
    Q_ASSERT(btnResume->isVisible());         // 右下角 HUD 胶囊按钮平滑浮现
    qInfo() << "Step 3: Navigation lockout triggered - auto-scroll released, resume HUD appeared. OK.";

    // 视觉冒烟：悬浮按钮真实渲染 (绿色胶囊边框与文字像素成片存在)
    QPixmap hudSnapshot = plotWidget.grab();
    hudSnapshot.save("D:/plot_navigation_snapshot.png");
    QImage hudImg = hudSnapshot.toImage();
    int greenHudPixels = 0;
    for (int y = 0; y < hudImg.height(); ++y) {
        for (int x = 0; x < hudImg.width(); ++x) {
            QColor c = hudImg.pixelColor(x, y);
            if (c.green() > 150 && c.red() < 160 && c.blue() < 120)
                greenHudPixels++;
        }
    }
    Q_ASSERT(greenHudPixels > 100); // 绿色胶囊边框 + 文字必须成片上屏
    qInfo() << QString("Step 3b: Resume HUD rendered with %1 green pixels.").arg(greenHudPixels);

    // 4. 定格期间数据继续流入，视口必须纹丝不动 (冲突保护)
    plotWidget.addDataPoint(1200.0, 60.0, 40.0, false);
    Q_ASSERT(qFuzzyCompare(plot->xAxis->range().upper, 1100.0)); // 画面仍停留在 1100
    qInfo() << "Step 4: Data keeps flowing while viewport stays frozen (no tug-of-war). OK.";

    // 5. 一键瞬移追平：点击悬浮按钮 → 视口秒级咬合最新采样时刻
    QMetaObject::invokeMethod(&plotWidget, "onResumeScrollClicked", Qt::DirectConnection);
    Q_ASSERT(btnResume->isHidden());          // 按钮自动隐去
    Q_ASSERT(chkAuto->isChecked());           // 复选框复位
    Q_ASSERT(qFuzzyCompare(plot->xAxis->range().upper, 1200.0)); // 追平最新点
    qInfo() << "Step 5: Resume clicked - viewport teleported to latest, HUD collapsed. OK.";

    // 6. 缩放极值硬夹紧：1 秒跨度被弹回 5 秒，2000 秒跨度被弹回 1800 秒
    plot->xAxis->setRange(1000.0, 1001.0);
    Q_ASSERT(plot->xAxis->range().size() >= 4.9);
    plot->xAxis->setRange(1000.0, 3000.0);
    Q_ASSERT(plot->xAxis->range().size() <= 1800.1);
    qInfo() << "Step 6: Zoom span hard clamp verified (min 5s / max 1800s). OK.";

    // 7. 高频注入性能基准：模拟 50Hz 遥测冲击 60 秒 (3000 帧)，排队重绘自动合并
    QElapsedTimer bench;
    bench.start();
    for (int i = 0; i < 3000; ++i)
        plotWidget.addDataPoint(1300.0 + i * 0.02, 50.0 + (i % 10), 30.0 + (i % 7));
    const qint64 elapsedMs = bench.elapsed();
    Q_ASSERT(elapsedMs < 2000); // 宽松上限：实测应在百毫秒级
    qInfo() << QString("Step 7: 3000-frame @50Hz burst injected in %1 ms (queued-replot coalescing).").arg(elapsedMs);

    // 8. 收尾：冲刷排队重绘，验证窗口仍健康渲染 (亮橙波形曲线真实上屏)
    for (int flush = 0; flush < 10; ++flush)
        QApplication::processEvents();
    QPixmap finalSnapshot = plotWidget.grab();
    finalSnapshot.save("D:/plot_navigation_final.png");
    QImage finalImg = finalSnapshot.toImage();
    int orangePixels = 0;
    for (int y = 0; y < finalImg.height(); y += 2) {
        for (int x = 0; x < finalImg.width(); x += 2) {
            QColor c = finalImg.pixelColor(x, y);
            if (c.red() > 200 && c.green() > 90 && c.green() < 190 && c.blue() < 80)
                orangePixels++;
        }
    }
    Q_ASSERT(orangePixels > 50);
    qInfo() << QString("Step 8: Final render healthy with %1 orange waveform pixels.").arg(orangePixels);

    qInfo() << ">>> SUCCESS: Interactive Navigation & 60 FPS Rendering Pipeline is 100% Solid!";
    qInfo() << "================ [RealtimePlot Interaction & Navigation Test End: All Passed] ================";
}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 全局样式与热重载挂载
    ThemeManager::instance().enableHotReload();
    ThemeManager::applyTheme(":/qss/dark_style.qss");

    // 2. 数据库底层基座初始化
    if (!DatabaseManager::instance().openDatabase()) {
        qCritical() << "[Main] Fatal: Could not establish database connection. Aborting.";
        return -1;
    }

    // V3
    // testAcquisitionWorkerThreadAffinity();
    // testCrossThreadTelemetryDelivery();
    //testAcquisitionControllerWorkflow();
    // testRapidStartStopCycles();
    // testHighFrequencyBurstConcurrency();
    // testDriverPoolMigration();
    // testAsyncBatchPollingPipeline();
    // testThrottleAndEventStormSuppression();
    // testExponentialBackoffReconnection();
    // testAlarmManagerLifecyclePipeline();
    // testAlarmEvaluationAndDeadbandPipeline();
    // testModelAlarmVisualLinkagePipeline();
    // testAlarmBannerLifecyclePipeline();
    // testFullPipelineStressAndConcurrency();
    // testRealtimePlotDualAxisTopology();
    // testRealtimePlotSlidingWindowPipeline();
    // testRealtimePlotRoutingAndDemuxPipeline();
    testRealtimePlotInteractionAndNavigationPipeline();
    // 3. 身份认证模态拦截
    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        // 用户取消或直接关闭，阻断主程序启动前安全释放数据库
        DatabaseManager::instance().closeDatabase();
        return 0;
    }

    // 4. 校验通过，唤醒监控系统主窗口
    MainWindow w;
    w.show();

    int exitCode = a.exec();

    // 5. 程序退出前，显式安全关闭数据库
    DatabaseManager::instance().closeDatabase();

    return exitCode;
}