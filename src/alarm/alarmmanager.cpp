#include "alarmmanager.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

AlarmManager &AlarmManager::instance()
{
    static AlarmManager s_instance;
    return s_instance;
}

void AlarmManager::postAlarm(const AlarmEvent &event)
{
    // 防溢出
    if(m_alarmHistory.size()>=m_maxCapacity)
        m_alarmHistory.removeFirst();
    m_alarmHistory.append(event);

    qWarning() << QString("[AlarmManager] ⚠ Triggered: [%1] %2 (Value: %3, Limit: %4)")
                      .arg(event.deviceId)
                      .arg(event.description)
                      .arg(event.triggerValue)
                      .arg(event.thresholdValue);
    emit alarmTriggered(event);

}

void AlarmManager::acknowledgeAlarm(const QString &alarmId)
{
    bool found=false;
    for(auto &event:m_alarmHistory)
    {
        if(event.alarmId==alarmId)
        {
            if(!event.acknowledged)
            {
                event.acknowledged=true;
                found=true;
                qInfo() << QString("[AlarmManager] 🔕 Acknowledged by operator: %1").arg(alarmId);
            }
            break;
        }
    }
    if(found)
        emit alarmAcknowledged(alarmId);
}

QList<AlarmEvent> AlarmManager::getActiveAlarms() const
{
    QList<AlarmEvent> activeList;
    for(const auto &event:m_alarmHistory)
    {
        if(!event.acknowledged)
            activeList.append(event);
    }
    return activeList;
}

AlarmManager::AlarmManager(QObject *parent)
    : QObject{parent}
{}

void AlarmManager::setMaxHistoryCapacity(int capacity)
{
    m_maxCapacity = qMax(1, capacity);
    while (m_alarmHistory.size() > m_maxCapacity) {
        m_alarmHistory.removeFirst();
    }
}

void AlarmManager::clearHistory()
{
    m_alarmHistory.clear();
}

void AlarmManager::setTemperatureThresholds(double warning, double critical, double deadband)
{
    m_tempWarningLimit = warning;
    m_tempCriticalLimit = critical;
    m_tempDeadband = qMax(0.1, deadband);
}



bool AlarmManager::loadThresholdsFromDb()
{
    // 1. 设置默认安全兜底值 (Fallbacks)
    double tempWarn = 75.0;
    double tempCrit = 85.0;
    double deadband = 1.0;
    double cpuCrit  = 90.0;

    QSqlQuery query("SELECT config_key, config_value FROM t_sys_config");
    if (!query.exec()) {
        qWarning() << "[AlarmManager] Failed to query t_sys_config, using defaults:" << query.lastError().text();
        setTemperatureThresholds(tempWarn, tempCrit, deadband);
        setCpuThreshold(cpuCrit);
        return false;
    }

    // 2. 遍历参数表动态提取
    while (query.next()) {
        QString key = query.value(0).toString();
        QString val = query.value(1).toString();

        if (key == "Alarm_Temp_Warning") {
            tempWarn = val.toDouble();
        } else if (key == "Alarm_Temp_Critical") {
            tempCrit = val.toDouble();
        } else if (key == "Alarm_Temp_Deadband") {
            deadband = val.toDouble();
        } else if (key == "Alarm_Cpu_Critical") {
            cpuCrit  = val.toDouble();
        }
    }

    // 3. 应用参数生效
    setTemperatureThresholds(tempWarn, tempCrit, deadband);
    setCpuThreshold(cpuCrit);

    qInfo() << QString("[AlarmManager] Thresholds loaded: TempWarn=%1℃, TempCrit=%2℃, Deadband=%3℃, CpuCrit=%4%")
                   .arg(tempWarn).arg(tempCrit).arg(deadband).arg(cpuCrit);
    return true;
}

AlarmLevel AlarmManager::getDeviceAlarmLevel(const QString &deviceId) const
{
    auto it=m_deviceStates.constFind(deviceId);
    if(it!=m_deviceStates.constEnd())
        return static_cast<AlarmLevel>(qMax(static_cast<int>(it.value().tempLevel),static_cast<int>(it.value().cpuLevel)));
    return AlarmLevel::Info;
}

void AlarmManager::evaluateBatch(const QList<TelemetryFrame> &batch)
{
    for (const auto &frame : batch) {
        evaluateTelemetry(frame);
    }
}

void AlarmManager::evaluateTelemetry(const TelemetryFrame &frame)
{
    if (!frame.commSuccess) return; // 通信失败由驱动与重连状态机处理

    auto &state = m_deviceStates[frame.deviceId];
    const double temp = frame.temperature;

    // ==========================================
    // 1. 温度维度：三级状态机与回差消抖转移矩阵
    // ==========================================
    switch (state.tempLevel) {
    case AlarmLevel::Info: { // 当前处于 Normal 正常阶段
        if (temp >= m_tempCriticalLimit) {
            state.tempLevel = AlarmLevel::Critical;
            AlarmEvent ev;
            ev.alarmId = QString("ALM-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(frame.deviceId);
            ev.deviceId = frame.deviceId;
            ev.level = AlarmLevel::Critical;
            ev.description = QString("模具温度发生严重过热: %1 ℃ (极值: %2 ℃)").arg(temp, 0, 'f', 1).arg(m_tempCriticalLimit);
            ev.triggerValue = temp;
            ev.thresholdValue = m_tempCriticalLimit;
            ev.timestamp = frame.timestamp;
            postAlarm(ev);
        } else if (temp >= m_tempWarningLimit) {
            state.tempLevel = AlarmLevel::Warning;
            AlarmEvent ev;
            ev.alarmId = QString("ALM-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(frame.deviceId);
            ev.deviceId = frame.deviceId;
            ev.level = AlarmLevel::Warning;
            ev.description = QString("模具温度偏高预警: %1 ℃ (上限: %2 ℃)").arg(temp, 0, 'f', 1).arg(m_tempWarningLimit);
            ev.triggerValue = temp;
            ev.thresholdValue = m_tempWarningLimit;
            ev.timestamp = frame.timestamp;
            postAlarm(ev);
        }
        break;
    }
    case AlarmLevel::Warning: { // 当前处于 Warning 阶段
        if (temp >= m_tempCriticalLimit) {
            // 越界升级
            state.tempLevel = AlarmLevel::Critical;
            AlarmEvent ev;
            ev.alarmId = QString("ALM-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(frame.deviceId);
            ev.deviceId = frame.deviceId;
            ev.level = AlarmLevel::Critical;
            ev.description = QString("温度持续恶化升级为严重报警: %1 ℃").arg(temp, 0, 'f', 1);
            ev.triggerValue = temp;
            ev.thresholdValue = m_tempCriticalLimit;
            ev.timestamp = frame.timestamp;
            postAlarm(ev);
        } else if (temp < (m_tempWarningLimit - m_tempDeadband)) {
            // 安全跌破死区下限，彻底解除
            state.tempLevel = AlarmLevel::Info;
            AlarmEvent ev;
            ev.alarmId = QString("ALM-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(frame.deviceId);
            ev.deviceId = frame.deviceId;
            ev.level = AlarmLevel::Info;
            ev.description = QString("模具温度已降温恢复正常: %1 ℃").arg(temp, 0, 'f', 1);
            ev.triggerValue = temp;
            ev.thresholdValue = m_tempWarningLimit;
            ev.timestamp = frame.timestamp;
            postAlarm(ev);
        }
        break;
    }
    case AlarmLevel::Critical: { // 当前处于 Critical 阶段
        // 只有跌破 (Critical - 死区) 才能降级
        if (temp < (m_tempCriticalLimit - m_tempDeadband)) {
            if (temp < (m_tempWarningLimit - m_tempDeadband)) {
                // 瞬间断崖式降至正常
                state.tempLevel = AlarmLevel::Info;
                AlarmEvent ev;
                ev.alarmId = QString("ALM-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(frame.deviceId);
                ev.deviceId = frame.deviceId;
                ev.level = AlarmLevel::Info;
                ev.description = QString("模具温度完全恢复正常: %1 ℃").arg(temp, 0, 'f', 1);
                ev.triggerValue = temp;
                ev.thresholdValue = m_tempWarningLimit;
                ev.timestamp = frame.timestamp;
                postAlarm(ev);
            } else {
                // 降级为预警
                state.tempLevel = AlarmLevel::Warning;
                AlarmEvent ev;
                ev.alarmId = QString("ALM-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(frame.deviceId);
                ev.deviceId = frame.deviceId;
                ev.level = AlarmLevel::Warning;
                ev.description = QString("模具严重过热已排除，当前处于偏高预警区: %1 ℃").arg(temp, 0, 'f', 1);
                ev.triggerValue = temp;
                ev.thresholdValue = m_tempWarningLimit;
                ev.timestamp = frame.timestamp;
                postAlarm(ev);
            }
        }
        break;
    }
    }
}