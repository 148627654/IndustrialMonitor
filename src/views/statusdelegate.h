#ifndef STATUSDELEGATE_H
#define STATUSDELEGATE_H

#include <QStyledItemDelegate>

/**
 * @brief 运行状态列自绘委托 (LED 指示灯 + 状态文字)
 * @details 以 荧光绿/亮黄/绯红/离线灰 圆点外加半透明柔光光晕绘制设备运行状态，
 *          纯 paint 绘制、不提供编辑器，与表格只读契约严格对齐；
 *          仅挂载于 TableView 的状态列，由视图按需逐格回调绘制。
 * @author Yuquan Guo
 * @date 2026-09
 */
class StatusDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit StatusDelegate(QObject *parent = nullptr);



    // QAbstractItemDelegate interface
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // STATUSDELEGATE_H
