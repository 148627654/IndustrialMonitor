#include "deviceproxymodel.h"

#include "devicetablemodel.h"

#include <QVariant>

DeviceProxyModel::DeviceProxyModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{
    setDynamicSortFilter(true);                 //动态监听过滤重排
}

void DeviceProxyModel::setSearchKeyword(const QString &keyword)
{
    m_keyword=keyword.trimmed();
    invalidateFilter();                         // 触发重新过滤计算
}

bool DeviceProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if(m_keyword.isEmpty())
        return true;
    QModelIndex idindex=sourceModel()->index(source_row,DeviceTableModel::Col_Id,source_parent);
    QModelIndex nameindex=sourceModel()->index(source_row,DeviceTableModel::Col_Name,source_parent);
    QModelIndex ipindex=sourceModel()->index(source_row,DeviceTableModel::Col_Ip,source_parent);

    QString id=idindex.data().toString();
    QString name=nameindex.data().toString();
    QString ip=ipindex.data().toString();

    return (id.contains(m_keyword,Qt::CaseInsensitive)||
            name.contains(m_keyword,Qt::CaseInsensitive)||
            ip.contains(m_keyword,Qt::CaseInsensitive));
}

bool DeviceProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    int col = source_left.column();
    QVariant leftData=sourceModel()->data(source_left,Qt::DisplayRole);
    QVariant rightData=sourceModel()->data(source_right,Qt::DisplayRole);

    switch (col) {
    case DeviceTableModel::Col_Port:
        return leftData.toInt()<rightData.toInt();
    case DeviceTableModel::Col_Temperature:
    case DeviceTableModel::Col_Cpu:
    case DeviceTableModel::Col_Memory:
    {

        double v1=leftData.toString().remove(" °C").remove(" %").toDouble();
        double v2=rightData.toString().remove(" °C").remove(" %").toDouble();
        return v1<v2;
    }
    default:
        return QSortFilterProxyModel::lessThan(source_left,source_right);
    }
}

