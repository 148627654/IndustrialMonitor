#ifndef ACQUISITIONWORKER_H
#define ACQUISITIONWORKER_H

#include <DeviceDef.h>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <TelemetryDef.h>
#include <atomic> // C++11 无锁原子库
#include <map>
#include <memory>
#include "idevice.h"
/**
 * @brief 后台高并发数据采集工作者 (Worker-Object 架构基座)
 * @details 继承自 QObject，设计为通过 moveToThread 运行在专有采集子线程中。
 *          负责在后台事件循环中统一调度硬件驱动池的异步读写，彻底隔离 GUI 线程。
 * @author Yuquan Guo
 * @date 2026-10
 */



class AcquisitionWorker : public QObject
{
    Q_OBJECT
public:
    explicit AcquisitionWorker(QObject *parent = nullptr);
    ~AcquisitionWorker() override;

    // 当前工作者运行状态
    bool isRunning() const { return m_running; }
    // 获取当前已经发出的帧总数 and 重置
    quint64 framesEmitted() const { return m_framesEmitted.load(std::memory_order_relaxed); }
    void resetStatistics() { m_framesEmitted.store(0, std::memory_order_relaxed); }

public slots:
    // 在子线程上下文中执行，
    void startWork(const QList<DeviceInfo> &devices);           // 负责开启心跳
    void stopWork();            // 负责安全关停
    void setInterval(int ms); // 动态跨线程调频
    // 测试槽：向指定设备注入/排除通信故障 (路由到驱动级故障钩子)
    void injectFault(const QString &deviceId, bool faultOn);

private slots:
    void onPollTimeout();     // 心跳轮询触发槽
signals:
    // 转移异步信号
    void started();                         // 子线程就绪
    void stopped();                         // 子线程停工
    void statusMessage(const QString &msg); // 线程内部日志广播

    // ==================== [V3-Day 2 新增：跨线程遥测数据总线信号] ====================
    // 单帧就绪信号 (面向实时告警与即时状态)
    void telemetryDataReady(const TelemetryFrame &frame);
    void deviceStateChanged(const QString &deviceId, IDevice::ConnectState state);

    // 批量帧就绪信号 (面向高频表格批量刷新与节流合并)
    void telemetryBatchReady(const QList<TelemetryFrame> &frames);
private:
    void loadDrivers(const QList<DeviceInfo> &devices);
    void unloadDrivers();

    bool m_running;
    int    m_intervalMs{ 1000 };
    QTimer *m_pollTimer{ nullptr }; // 必须延迟在 startWork() 内部创建！
    quint64 m_tickCount{ 0 };


    // 调度管理池再子线程内部
    std::map<QString,std::unique_ptr<IDevice>> m_drivers;
    // 无锁原子发射计数器
    std::atomic<quint64> m_framesEmitted{ 0 };


    struct ReconnectContext {
        int retryCount = 0;                  // 当前已连续失败重试的次数
        qint64 nextRetryTimestampMs = 0;     // 下一次允许重试调用的绝对时间戳 (QDateTime::currentMSecsSinceEpoch)
        bool isRetrying = false;             // 是否正处于重连退避观察期
    };

    std::map<QString, ReconnectContext> m_reconnectContexts;
    const int MAX_RETRY_ATTEMPTS = 5;
    const int BASE_BACKOFF_MS    = 1000;
    const int MAX_BACKOFF_MS     = 16000;
private:
    void processReconnections(qint64 nowMs);
    void triggerReconnection(const QString &deviceId,qint64 nowMs);
};

#endif // ACQUISITIONWORKER_H
