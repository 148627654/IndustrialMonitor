#ifndef DEVICEPROXYMODEL_H
#define DEVICEPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QString>

/**
 * @brief 设备列表排序过滤代理模型 (MVC 视图层适配器)
 * @details 挂载于 DeviceTableModel 之上，对外屏蔽底层列布局细节：
 *          setSearchKeyword 对 设备编号/名称/IP 三列执行大小写不敏感联合过滤；
 *          重写 lessThan 使 端口/温度/CPU/内存 列按数值比较而非字符串字典序，
 *          并开启 setDynamicSortFilter 动态监听数据变更即时重排。
 * @author Yuquan Guo
 * @date 2026-09
 */
class DeviceProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DeviceProxyModel(QObject *parent = nullptr);
    // 设置搜索关键字 (槽函数)
    void setSearchKeyword(const QString &keyword);
    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

    // QSortFilterProxyModel interface
protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;
private:
    QString m_keyword;
};

#endif // DEVICEPROXYMODEL_H
