#include "statusdelegate.h"

#include <QApplication>
#include <QPainter>

StatusDelegate::StatusDelegate(QObject *parent)
    : QStyledItemDelegate{parent}
{}

void StatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 先保护
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing,true);                //开启抗锯齿

    //绘制默认背景
    QStyleOptionViewItem opt=option;
    initStyleOption(&opt,index);
    opt.text.clear();                                       //清空画布
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem,&opt,painter);

    // 提取颜色
    QString text=index.data(Qt::DisplayRole).toString();
    QColor coreColor;
    QColor glowColor;

    if (text == "正常") {
        coreColor = QColor("#00E676");             // 荧光绿
        glowColor = QColor(0, 230, 118, 90);       // 半透明外圈柔光
    } else if (text == "警告") {
        coreColor = QColor("#FFD600");             // 亮黄
        glowColor = QColor(255, 214, 0, 90);
    } else if (text == "故障") {
        coreColor = QColor("#FF1744");             // 绯红
        glowColor = QColor(255, 23, 68, 90);
    } else {
        coreColor = QColor("#9E9E9E");             // 离线灰
        glowColor = Qt::transparent;
    }
    //获取发光的坐标中心
    QRect rect=option.rect;
    int centerY=rect.top()+rect.height()/2;
    int circleX=rect.left()+18;

    //绘制外层光晕
    if(glowColor!=Qt::transparent)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(glowColor);
        painter->drawEllipse(QPoint(circleX,centerY),6,6);
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(coreColor);
    painter->drawEllipse(QPoint(circleX,centerY),4,4);


    //绘制文字
    QRect textRect(circleX+12,rect.top(),rect.width()-(circleX+12-rect.left()),rect.height());

    if(option.state & QStyle::State_Selected)
        painter->setPen(QColor("#FFFFFF"));
    else
        painter->setPen(QColor("#DCDCDC"));

    painter->setFont(option.font);
    painter->drawText(textRect,Qt::AlignLeft|Qt::AlignVCenter,text);

    painter->restore();
}
