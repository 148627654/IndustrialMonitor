#include "progressbardelegate.h"

#include <QApplication>
#include <QPainter>
#include <algorithm>

ProgressBarDelegate::ProgressBarDelegate(QObject *parent)
    : QStyledItemDelegate{parent}
{}

void ProgressBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 保护
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing,true);    //抗锯齿

    //绘制默认背景
    QStyleOptionViewItem opt=option;
    initStyleOption(&opt,index);
    opt.text.clear();
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem,&opt,painter);

    //数值截取0-100
    QString str=index.data(Qt::DisplayRole).toString();
    double value=str.remove(" %").toDouble();
    value=std::max(0.0,std::min(100.0,value));

    // 三段色温
    QColor barcolor;
    if(value<60.0)
        barcolor=QColor("#0E639C");         //正常 蓝色
    else if(value<85.0)
        barcolor=QColor("#FFD600");         //警告 亮黄
    else
        barcolor=QColor("#FF1744");         //危险 红色

    // 进度条计算
    QRect rect=opt.rect;
    int marginH=12;
    int barHeight=16;
    int barY=rect.top()+(rect.height()-barHeight)/2;
    QRect bgRect(rect.left()+marginH,barY,rect.width()-marginH*2,barHeight);

    //绘制背景
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#252526"));
    painter->drawRoundedRect(bgRect,4,4);

    // 绘制实际填充的滑条
    int fillWidth=static_cast<int>(bgRect.width()*(value/100.0));
    if(fillWidth>0)
    {
        QRect fillRect(bgRect.left(),bgRect.top(),fillWidth,bgRect.height());
        painter->setBrush(barcolor);
        painter->drawRoundedRect(fillRect,4,4);
    }

    // 居中绘制纯白加粗文本
    painter->setPen("#FFFFFF");
    QFont font=option.font;
    font.setPointSize(9);
    font.setBold(true);
    painter->setFont(font);

    QString text=QString::number(value,'f',1)+" %";
    painter->drawText(bgRect,Qt::AlignCenter,text);

    // 复原
    painter->restore();
}
