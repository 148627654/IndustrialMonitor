#ifndef ROWGLOWDELEGATE_H
#define ROWGLOWDELEGATE_H

#include <QStyledItemDelegate>

/**
 * @brief 整行告警微光委托 (Row Glow Delegate)
 * @details 工程启用 QSS 后，QStyleSheetStyle 会接管条目背景绘制并吞掉模型的
 *          Qt::BackgroundRole 画笔。本委托在样式绘制之前手工铺设半透明微光底层，
 *          保证告警整行背景在真实视口中稳定呈现。
 */
class RowGlowDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit RowGlowDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

#endif // ROWGLOWDELEGATE_H
