#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QObject>
#include "AlarmDef.h"
#include <QHash>
#include <QList>
#include <TelemetryDef.h>

/**
 * @brief 全局工业告警管理调度中枢 (Singleton)
 * @details 负责全厂告警事件的分发广播、环形历史缓冲区维护与操作员消音生命周期追踪。
 */

class AlarmManager : public QObject
{
    Q_OBJECT
public:
    static AlarmManager& instance();

    // 提交新的警告事件
    void postAlarm(const AlarmEvent &event);
    // 管理员的确认操作
    void acknowledgeAlarm(const QString &alarmId);
    // 查询当前所有活动
    QList<AlarmEvent> getActiveAlarms() const;
    // 获取环型变量历史记录
    QList<AlarmEvent> getHistoricalAlarms() const { return m_alarmHistory; }
    // 设置环形缓冲区最大容量
    void setMaxHistoryCapacity(int capacity);
    int maxHistoryCapacity() const { return m_maxCapacity; }

    void clearHistory();
    // 评估单帧遥测数据
    void evaluateTelemetry(const TelemetryFrame &frame);
    // 批量评估遥测帧
    void evaluateBatch(const QList<TelemetryFrame> &batch);
    // 动态配置阈值与死区
    void setTemperatureThresholds(double warning, double critical, double deadband = 1.0);
    void setCpuThreshold(double critical){m_cpuCriticalLimit=critical;}
    // 从 SQLite 系统配置表加载报警阈值与回差死区
    bool loadThresholdsFromDb();
    // 获取指定设备当前最高警告级别
    AlarmLevel getDeviceAlarmLevel(const QString &deviceId) const;

signals:
    // 新警告触发广播
    void alarmTriggered(const AlarmEvent &event);
    // 警告已经被管理权确认广播
    void alarmAcknowledged(const QString &alarmId);
private:
    explicit AlarmManager(QObject *parent = nullptr);
    ~AlarmManager() override = default;
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;
private:
    QList<AlarmEvent> m_alarmHistory; // 内存环形历史缓冲区
    int m_maxCapacity{500};           // 最大保留记录数 (防内存溢出)

    struct DeviceAlarmState
    {
        AlarmLevel tempLevel{AlarmLevel::Info};
        AlarmLevel cpuLevel{AlarmLevel::Info};
    };
    QHash<QString, DeviceAlarmState> m_deviceStates; // 设备编号 -> 当前告警状态
    double m_tempWarningLimit{75.0};
    double m_tempCriticalLimit{85.0};
    double m_tempDeadband{1.0};
    double m_cpuCriticalLimit{90.0};
};

#endif // ALARMMANAGER_H
