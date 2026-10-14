#include "progressbardelegate.h"

#include <QApplication>
#include <QPainter>
#include <algorithm>

ProgressBarDelegate::ProgressBarDelegate(QObject *parent)
    : QStyledItemDelegate{parent}
{}

void ProgressBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 1. 现场保护与抗锯齿
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 2. 绘制默认背景 (保留整行选中态与悬浮态)
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

    // 2.5 后铺半透明告警微光 (防斑马纹覆盖；进度条与文字随后绘制不受染色影响)
    const QVariant bg=index.data(Qt::BackgroundRole);
    if(bg.canConvert<QBrush>())
        painter->fillRect(option.rect,bg.value<QBrush>());

    // 3. 数值截取 0.0 ~ 100.0
    QString str = index.data(Qt::DisplayRole).toString();
    double value = str.remove(" %").toDouble();
    value = std::max(0.0, std::min(100.0, value));

    // 4. 几何计算 (居中槽位)
    QRect rect = opt.rect;
    int marginH = 12;
    int barHeight = 16;
    int barY = rect.top() + (rect.height() - barHeight) / 2;
    QRect bgRect(rect.left() + marginH, barY, rect.width() - marginH * 2, barHeight);

    // 5. 绘制底槽轨道 (暗黑背景槽)
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#252526"));
    painter->drawRoundedRect(bgRect, 4, 4);

    // 6. 核心：三段告警色阶渐变填充 (墨绿/工业蓝 -> 亮黄 -> 绯红)
    int fillWidth = static_cast<int>(bgRect.width() * (value / 100.0));
    if (fillWidth > 0)
    {
        QRect fillRect(bgRect.left(), bgRect.top(), fillWidth, bgRect.height());

        // 升级点：给进度条加一点微微的线性渐变，瞬间提升工业科技质感！
        QLinearGradient gradient(fillRect.topLeft(), fillRect.topRight());
        if (value < 70.0) {
            // 正常：工业科技蓝 / 墨绿
            gradient.setColorAt(0.0, QColor("#0E639C"));
            gradient.setColorAt(1.0, QColor("#1177BB"));
        } else if (value < 85.0) {
            // 预警：警示亮黄
            gradient.setColorAt(0.0, QColor("#E6A23C"));
            gradient.setColorAt(1.0, QColor("#F5C35D"));
        } else {
            // 危险：严重绯红
            gradient.setColorAt(0.0, QColor("#F56C6C"));
            gradient.setColorAt(1.0, QColor("#F89898"));
        }

        painter->setBrush(gradient);
        painter->drawRoundedRect(fillRect, 4, 4);
    }

    // 7. 居中绘制纯白加粗文本
    painter->setPen(QColor("#FFFFFF"));
    QFont font = option.font;
    font.setPointSize(9);
    font.setBold(true);
    painter->setFont(font);

    QString text = QString::number(value, 'f', 1) + " %";
    painter->drawText(bgRect, Qt::AlignCenter, text);

    // 8. 现场复原
    painter->restore();
}
