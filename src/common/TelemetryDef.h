#ifndef TELEMETRYDEF_H
#define TELEMETRYDEF_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QDebug>

/**
 * @brief 工业设备单次采集原子遥测帧 (只读高频值对象)
 * @details 采用纯值语义设计，支持寄存器原始快照与工程量双轨制，
 *          完全适配 Qt::QueuedConnection 跨线程安全传递。
 * @author Yuquan Guo
 * @date 2026-09
 */

struct TelemetryFrame
{
    QString   deviceId;                 // 设备唯一资产编号 (如 "DEV-1001")
    QDateTime timestamp;                // 采集时间戳 (物理时钟)
    double    temperature{ 0.0 };       // 解码后的物理温度 (℃)
    double    cpuUsage{ 0.0 };          // CPU 占用率 (0~100%)
    double    memoryUsage{ 0.0 };       // 内存占用率 (0~100%)
    bool      commSuccess{ true };      // 本帧通信健康度 (true: 成功, false: 离线/异常)
    quint16   rawRegisters[5]{ 0 };     // 原始保持寄存器快照 (用于底层排障)

    QString toString() const
    {
        return QString("[%1 @ %2] Temp: %3 ℃ | CPU: %4 % | Mem: %5 % | Link: %6")
            .arg(deviceId)
            .arg(timestamp.toString("HH:mm:ss.zzz"))
            .arg(temperature, 0, 'f', 1)
            .arg(cpuUsage, 0, 'f', 1)
            .arg(memoryUsage, 0, 'f', 1)
            .arg(commSuccess ? "OK" : "ERR");
    }
};

inline QDebug operator<<(QDebug dbg,const TelemetryFrame &frame)
{
    QDebugStateSaver saver(dbg);
    dbg.noquote()<<frame.toString();
    return dbg;
}

Q_DECLARE_METATYPE(TelemetryFrame);
Q_DECLARE_METATYPE(QList<TelemetryFrame>);

#endif // TELEMETRYDEF_H
