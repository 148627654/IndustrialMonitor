#ifndef ACQUISITIONCONTROLLER_H
#define ACQUISITIONCONTROLLER_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QHash>
#include "common/TelemetryDef.h"
#include <atomic>
class AcquisitionWorker;

/**
 * @brief 工业采集引擎主控中枢 (Controller / Facade Pattern 单例)
 * @details 运行于 GUI 主线程，全权托管后台采集子线程生命周期、动态参数注入与信号透传中继。
 * @author Yuquan Guo
 * @date 2026-10
 */
class AcquisitionController : public QObject
{
    Q_OBJECT

public:
    enum class EngineState{
        Stopped,        // 已停止
        Starting,       // 正在启动
        Running,        // 正在运行
        Stopping,       // 正在停止
    };
    Q_ENUM(EngineState)
    static AcquisitionController& instance();

    // 初始化线程与工作者装配
    bool init();

    // 业务控制原语
    void start(int intervalMs = -1);
    void stop();
    void setInterval(int intervalMs);
    bool isRunning() const;
    EngineState state() const { return m_state; }
    quint64 framesEmitted() const;
    quint64 framesReceived() const {return m_framesReceived.load(std::memory_order_relaxed);}
    void resetStatistics();

    // 设置是否开启节流保护
    void setThrottleEnabled(bool enable);
    void setThrottleInterval(int ms);
    bool isThrottleEnable() const {return m_throttleEnabled;}

    // 暴露底层工作者指针 (仅供测试槽跨线程 invokeMethod 注入指令)
    AcquisitionWorker *worker() const { return m_worker; }

signals:
    // ==================== [中继透传数据总线信号] ====================
    void telemetryDataReady(const TelemetryFrame &frame);
    void telemetryBatchReady(const QList<TelemetryFrame> &frames);
    void statusMessage(const QString &msg);
    void engineStarted();
    void engineStopped();

    void throttledBatchReady(const QList<TelemetryFrame> &batch);
private slots:
    // 接收Worker原始批量数据并进行最新值合并
    void onWorkerBatchReady(const QList<TelemetryFrame> &batch);
    // 截留渲染时钟超时，将合并桶刷入UI
    void onThrottleTimeout();
private:
    AcquisitionController(QObject *parent = nullptr);
    ~AcquisitionController() override;
    AcquisitionController(const AcquisitionController&) = delete;
    AcquisitionController& operator=(const AcquisitionController&) = delete;

    int loadDefaultIntervalFromDb();

    QThread            *m_thread{ nullptr };
    AcquisitionWorker  *m_worker{ nullptr };
    bool               m_initialized{ false };
    EngineState        m_state{ EngineState::Stopped };

    std::atomic<quint64> m_framesReceived;
    quint64 m_lastEmittedCount{ 0 };

    QTimer *m_throttleTimer=nullptr;
    int m_throttleIntervalMs=33;        // 默认 33ms (约 30 FPS)
    bool m_throttleEnabled=true;
    QHash<QString,TelemetryFrame> m_pendingFrames;

};

#endif // ACQUISITIONCONTROLLER_H
