#ifndef ALARMDEF_H
#define ALARMDEF_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

/**
 * @brief 符合 ISA-18.2 工业标准的报警严重等级
 */
enum class AlarmLevel
{
    Info = 0,           // 提示信息系统状态变更
    Warning=1,          // 预警
    Critical=2          // 严重报警
};

/**
 * @brief 工业全域警告事件实体
 */
struct AlarmEvent
{
    QString alarmId;                // 警告唯一流水号
    QString deviceId;               // 警告设备编号
    QString deviceName;             // 警告设备名称
    AlarmLevel level{AlarmLevel::Warning};//警告级别
    QString description;            // 详细报警文案
    double triggerValue{0.0};       // 触发时的物理瞬态读数
    double thresholdValue{0.0};     // 工艺报警上限/下限
    QDateTime timestamp;            // 时间戳
    bool acknowledged{false};       // 管理员是否知晓
};

Q_DECLARE_METATYPE(AlarmEvent)
Q_DECLARE_METATYPE(QList<AlarmEvent>)
#endif // ALARMDEF_H
