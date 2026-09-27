#include "devicetablemodel.h"
#include <QRandomGenerator>
#include <algorithm>

DeviceTableModel::DeviceTableModel(QObject *parent)
    : QAbstractTableModel{parent}
{
    m_headers<<"设备编号"<<"设备名称"<<"IP地址"<<"端口"
              <<"运行状态"<<"温度(℃)"<<"CPU占用"<<"内存占用";
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

        // 核心技术：精准发射局部重绘信号 (只通知温度列到内存列重绘，极其高效)
        emit dataChanged(createIndex(row, Col_Temperature),
                         createIndex(row, Col_Memory));
    }
}

void DeviceTableModel::addDevice(const DeviceInfo &device)
{
    int row = m_devices.size();
    beginInsertRows(QModelIndex(), row, row);
    m_devices.append(device);
    endInsertRows();
}
