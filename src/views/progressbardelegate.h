#ifndef PROGRESSBARDELEGATE_H
#define PROGRESSBARDELEGATE_H

#include <QStyledItemDelegate>

/**
 * @brief 进度条列自绘委托 (数值进度条 + 三段色温告警)
 * @details 将 CPU/内存占用率绘制为圆角进度条：低于 60% 工控蓝、低于 85% 亮黄、
 *          其余绯红告警；数值先截断至 [0,100] 区间再居中叠加百分比文本，
 *          全程抗锯齿渲染。同一实例可同时挂载 CPU 与内存两列。
 * @author Yuquan Guo
 * @date 2026-09
 */
class ProgressBarDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ProgressBarDelegate(QObject *parent = nullptr);

    // QAbstractItemDelegate interface
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

};

#endif // PROGRESSBARDELEGATE_H
