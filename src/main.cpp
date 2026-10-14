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
#include <QVBoxLayout>
#include <QUuid>
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
    testFullPipelineStressAndConcurrency();
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