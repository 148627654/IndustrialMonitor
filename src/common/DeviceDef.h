#ifndef DEVICEDEF_H
#define DEVICEDEF_H
#include <QString>

enum class DeviceStatus
{
    Offline = 0, Normal = 1, Warning = 2, Fault = 3
};

struct DeviceInfo
{
    QString id;                 //设备编号
    QString name;               //设备名称
    QString ip;                 //IP地址
    int port=7988;                   //端口号
    DeviceStatus status=DeviceStatus::Offline;        //运行状态
    double temperature=0.0;         //温度
    double cpuUsage=0.0;            //CPU占用率
    double memoryUsage=0.0;         //内存占用率
};

#endif // DEVICEDEF_H

