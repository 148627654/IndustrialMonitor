#ifndef DEVICETABLEMODEL_H
#define DEVICETABLEMODEL_H

#include <QAbstractTableModel>
#include <QObject>
#include <QList>
#include <QStringList>
#include "../common/DeviceDef.h"

/**
 * @brief 工业设备核心数据表格模型 (MVC 架构数据源)
 * @details 负责维护设备连续内存列表 QList<DeviceInfo>，实现虚拟化按需索取渲染，
 *          严格履行 rowCount/columnCount/headerData/data 四大虚函数契约。
 *          写操作统一走 begin/end 系列宏通知视图层，局部刷新使用
 *          dataChanged 精准发射，保证 10000+ 行压测场景下的重绘效率。
 * @author Yuquan Guo
 * @date 2026-09
 */
class DeviceTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        Col_Id = 0,
        Col_Name,
        Col_Ip,
        Col_Port,
        Col_Status,
        Col_Temperature,
        Col_Cpu,
        Col_Memory,
        Col_Count
    };
    explicit DeviceTableModel(QObject *parent = nullptr);

private:
    QList<DeviceInfo> m_devices;
    QStringList m_headers;

    // QAbstractItemModel interface
public:
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    // 数据操作外部接口
    void setDeviceList(const QList<DeviceInfo> &devices);
    const DeviceInfo& getDevice(int row) const;
    void clear();

    void updateRandomData();

    const QList<DeviceInfo>& deviceList() const { return m_devices; }
    void addDevice(const DeviceInfo &device);
};

#endif // DEVICETABLEMODEL_H
