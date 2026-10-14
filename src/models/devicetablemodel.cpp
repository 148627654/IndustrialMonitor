#include "devicetablemodel.h"
#include "AlarmDef.h"
#include <QBrush>
#include <QColor>
#include <QRandomGenerator>
#include <alarmmanager.h>
#include <algorithm>

DeviceTableModel::DeviceTableModel(QObject *parent)
    : QAbstractTableModel{parent}
{
    m_headers<<"设备编号"<<"设备名称"<<"IP地址"<<"端口"
              <<"运行状态"<<"温度(℃)"<<"CPU占用"<<"内存占用";
}

void DeviceTableModel::rebuildIndexMap()
{
    m_idToRow.clear();
    for(int i=0;i<m_devices.size();++i)
        m_idToRow.insert(m_devices[i].id,i);
}

int DeviceTableModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid())return 0;
    return m_devices.size();
}

int DeviceTableModel::columnCount(const QModelIndex &parent) const
{
    if(parent.isValid())return 0;
    return Col_Count;
}

QVariant DeviceTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()||index.row()<0||index.row()>=m_devices.size())
        return QVariant();

    const DeviceInfo &dev=m_devices.at(index.row());

    // 整行警告微光背景色穿透 (预警→琥珀 #E6A23C / 故障→绯红 #F56C6C)
    if(role==Qt::BackgroundRole)
    {
        if(dev.status==DeviceStatus::Warning)
            return QBrush(QColor(230,162,60,28));
        else if(dev.status==DeviceStatus::Fault)
            return QBrush(QColor(245,108,108,22));
        return QVariant();
    }
    int col=index.column();
    //文本显示角色
    if(role==Qt::DisplayRole)
    {
        switch (col) {
        case Col_Id:return dev.id;
        case Col_Name :return dev.name;
        case Col_Ip:return dev.ip;
        case Col_Port :return dev.port;
        case Col_Status:
        {
            switch (dev.status) {
            case DeviceStatus::Normal:return "正常";
            case DeviceStatus::Warning:return "警告";
            case DeviceStatus::Fault:return "故障";
            case DeviceStatus::Offline:return "离线";
            }
            return "未知";
        }
        case Col_Temperature :return QString::number(dev.temperature,'f',1)+" °C";
        case Col_Cpu :return QString::number(dev.cpuUsage,'f',1)+" %";
        case Col_Memory :return QString::number(dev.memoryUsage,'f',1)+" %";
        default:
            break;
        }
    }

    //文字对其角色
    if(role==Qt::TextAlignmentRole)
    {
        if(col==Col_Id||col==Col_Name||col==Col_Ip)
            return QVariant(Qt::AlignLeft|Qt::AlignVCenter);
        return QVariant(Qt::AlignCenter);
    }
    // 强类型原始数据角色
    if(role==Qt::UserRole)
    {
        if (col == Col_Status)
            return static_cast<int>(dev.status); // 直接返回原生状态枚举整数值
        if (col == Col_Temperature) return dev.temperature;
        if (col == Col_Cpu)         return dev.cpuUsage;
        if (col == Col_Memory)      return dev.memoryUsage;
    }

    return QVariant();
}

//题头
QVariant DeviceTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation==Qt::Horizontal&&role==Qt::DisplayRole)
    {
        if(section>=0&&section<m_headers.size())
            return m_headers.at(section);
    }
    return QVariant();
}

void DeviceTableModel::setDeviceList(const QList<DeviceInfo> &devices)
{
    beginResetModel();
    m_devices=devices;
    rebuildIndexMap();
    endResetModel();
}

const DeviceInfo &DeviceTableModel::getDevice(int row) const
{
    return m_devices.at(row);
}

void DeviceTableModel::clear()
{
    beginResetModel();
    m_devices.clear();
    endResetModel();
}

void DeviceTableModel::updateRandomData()
{
    if (m_devices.isEmpty()) return;

    // 每次随机抽取 5 台设备产生微小的传感器数值跳变
    for (int k = 0; k < 5; ++k) {
        int row = QRandomGenerator::global()->bounded(m_devices.size());
        DeviceInfo &dev = m_devices[row];

        // 随机波动温度 (-0.5 ~ +0.5 ℃)
        double tempDelta = (QRandomGenerator::global()->bounded(100) - 50) / 100.0;
        dev.temperature = std::max(20.0, std::min(95.0, dev.temperature + tempDelta));

        // 随机波动 CPU ( -3% ~ +3% )
        double cpuDelta = (QRandomGenerator::global()->bounded(60) - 30) / 10.0;
        dev.cpuUsage = std::max(5.0, std::min(99.0, dev.cpuUsage + cpuDelta));

        // 随机波动内存 ( -1% ~ +1% )
        double memDelta = (QRandomGenerator::global()->bounded(20) - 10) / 10.0;
        dev.memoryUsage = std::max(10.0, std::min(95.0, dev.memoryUsage + memDelta));

        // 核心联动：随机泵数据同步送入告警状态机，就地跃迁状态列并点亮整行微光
        TelemetryFrame frame;
        frame.deviceId    = dev.id;
        frame.temperature = dev.temperature;
        frame.cpuUsage    = dev.cpuUsage;
        frame.memoryUsage = dev.memoryUsage;
        frame.commSuccess = true;
        frame.timestamp   = QDateTime::currentDateTime();
        AlarmManager::instance().evaluateTelemetry(frame);
        applyAlarmStatus(dev, AlarmManager::instance().getDeviceAlarmLevel(frame.deviceId));

        // 脏矩形覆盖整行：状态列、背景微光与数值列同步重绘
        emit dataChanged(createIndex(row, 0),
                         createIndex(row, Col_Count - 1),
                         {Qt::DisplayRole, Qt::BackgroundRole});
    }
}

void DeviceTableModel::addDevice(const DeviceInfo &device)
{
    int row = m_devices.size();
    beginInsertRows(QModelIndex(), row, row);
    m_devices.append(device);
    endInsertRows();
}

void DeviceTableModel::updateDevice(int row, const DeviceInfo &device)
{
    if (row < 0 || row >= m_devices.size()) return;
    m_devices[row]=device;
    emit dataChanged(createIndex(row,0),createIndex(row,Col_Count-1));
}

bool DeviceTableModel::removeDevice(int row)
{
    if (row < 0 || row >= m_devices.size()) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_devices.removeAt(row);
    endRemoveRows(); // 视口收缩并重绘

    return true;
}

void DeviceTableModel::applyAlarmStatus(DeviceInfo &dev, AlarmLevel level)
{
    // 规范映射：Critical→故障(绯红) / Warning→预警(琥珀) / Info→正常
    if (level == AlarmLevel::Critical)
        dev.status = DeviceStatus::Fault;
    else if (level == AlarmLevel::Warning)
        dev.status = DeviceStatus::Warning;
    else
        dev.status = DeviceStatus::Normal;
}

void DeviceTableModel::updateTelemetryBatch(const QList<TelemetryFrame> &frames)
{
    if (frames.isEmpty() || m_devices.isEmpty()) return;

    for (const auto &frame : frames) {
        auto it = m_idToRow.constFind(frame.deviceId);
        if (it == m_idToRow.constEnd()) continue;

        int row = it.value();
        DeviceInfo &dev = m_devices[row];

        // 1. 就地刷新物理数值
        dev.temperature = frame.temperature;
        dev.cpuUsage    = frame.cpuUsage;
        dev.memoryUsage = frame.memoryUsage;

        // 2. 核心联动：由 AlarmManager 告警状态机裁定视觉状态
        if (!frame.commSuccess) {
            dev.status = DeviceStatus::Offline;
        } else {
            applyAlarmStatus(dev, AlarmManager::instance().getDeviceAlarmLevel(frame.deviceId));
        }

        // 3. 脏矩形覆盖整行 (第 0 列到最后一列)，确保整行微光背景色与所有委托同步重绘
        QModelIndex topLeft = index(row, 0);
        QModelIndex bottomRight = index(row, columnCount(QModelIndex()) - 1);
        emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::BackgroundRole});
    }

}
