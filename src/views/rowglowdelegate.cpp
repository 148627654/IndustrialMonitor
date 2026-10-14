#include "rowglowdelegate.h"

#include <QPainter>

RowGlowDelegate::RowGlowDelegate(QObject *parent)
    : QStyledItemDelegate{parent}
{}

void RowGlowDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    // 1. 先交还样式绘制全部前景 (斑马纹/悬浮/选中/文本)
    QStyledItemDelegate::paint(painter, option, index);

    // 2. 后叠半透明告警微光：后绘保证不被斑马纹等样式背景覆盖 (α 仅 8%~11%，文字轻微染色可忽略)
    const QVariant bg = index.data(Qt::BackgroundRole);
    if (bg.canConvert<QBrush>())
    {
        painter->save();
        painter->fillRect(option.rect, bg.value<QBrush>());
        painter->restore();
    }
}
