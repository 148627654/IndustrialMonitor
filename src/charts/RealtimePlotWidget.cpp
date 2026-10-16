#include "RealtimePlotWidget.h"

#include "qcustomplot.h"
#include <QVBoxLayout>

RealtimePlotWidget::RealtimePlotWidget(QWidget *parent)
    : QWidget(parent)
{
    // 1. 装配双轴拓扑与曲线
    setupPlot();

    // 2. 注入工业暗黑主题
    setupDarkTheme();

    // 3. 全部配置完毕，统一只触发一次重绘 (杜绝配置期白斑闪烁)
    m_plot->replot();
}

RealtimePlotWidget::~RealtimePlotWidget() = default;

void RealtimePlotWidget::setupPlot()
{
    m_plot = new QCustomPlot(this);
    m_plot->setObjectName("realtimePlot");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_plot);

    // ==================== 曲线 0：模具温度 (左 Y 轴) ====================
    QCPGraph *tempGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    tempGraph->setName("模具温度");
    tempGraph->setPen(QPen(QColor("#FF9800"), 2));               // 高信噪比亮橙
    tempGraph->setBrush(QColor(255, 152, 0, 20));                // 下方半透明渐变面积光晕

    // ==================== 曲线 1：主轴负荷 (右 Y 轴) ====================
    m_plot->yAxis2->setVisible(true);                            // 显式激活右轴
    QCPGraph *loadGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis2);
    loadGraph->setName("主轴负荷");
    loadGraph->setPen(QPen(QColor("#00E5FF"), 2));               // 工业科技青蓝

    // ==================== 双 Y 轴独立物理量纲 ====================
    m_plot->yAxis->setLabel("模具温度 (℃)");
    m_plot->yAxis->setRange(0.0, 120.0);                         // 固定量程

    m_plot->yAxis2->setLabel("主轴负荷 (%)");
    m_plot->yAxis2->setRange(0.0, 100.0);                        // 固定量程

    // X 轴预留物理时间轴占位 (真实 hh:mm:ss 刻度待遥测时间戳接入后启用)
    m_plot->xAxis->setLabel("物理时间 (s)");
    m_plot->xAxis->setRange(0.0, 300.0);

    // 图例显示在图表右上角
    m_plot->legend->setVisible(true);
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
}

void RealtimePlotWidget::setupDarkTheme()
{
    // ==================== 画布底色：低眩光深灰黑 ====================
    m_plot->setBackground(QColor("#1A1A1A"));
    m_plot->axisRect()->setBackground(QColor("#121212"));

    // ==================== 坐标轴主线 / 刻度 / 标签配色 ====================
    const QColor axisBaseColor("#555555");
    const QColor tickColor("#777777");
    const QColor labelColor("#CCCCCC");
    const QColor gridColor(80, 80, 80, 80);                      // 半透明暗灰点阵网格

    const QList<QCPAxis *> axes = { m_plot->xAxis, m_plot->yAxis, m_plot->yAxis2, m_plot->xAxis2 };
    for (QCPAxis *axis : axes) {
        axis->setBasePen(QPen(axisBaseColor));
        axis->setTickPen(QPen(tickColor));
        axis->setSubTickPen(QPen(tickColor));
        axis->setTickLabelColor(labelColor);
        axis->setLabelColor(labelColor);
        axis->grid()->setPen(QPen(gridColor, 1, Qt::DotLine));
        axis->grid()->setSubGridPen(QPen(QColor(80, 80, 80, 40), 1, Qt::DotLine));
        axis->grid()->setZeroLinePen(QPen(QColor(80, 80, 80, 100)));
    }

    // ==================== 图例暗黑定制：半透明黑底 + 纯白文字 ====================
    m_plot->legend->setBrush(QColor(0, 0, 0, 160));
    m_plot->legend->setBorderPen(QPen(QColor("#555555")));
    m_plot->legend->setTextColor(Qt::white);
    QFont legendFont = m_plot->legend->font();
    legendFont.setPointSize(9);
    legendFont.setBold(true);
    m_plot->legend->setFont(legendFont);
}

void RealtimePlotWidget::appendTemperature(double timeSec, double celsius)
{
    if (!m_plot || m_plot->graphCount() < 1)
        return;
    m_plot->graph(0)->addData(timeSec, celsius);
    m_plot->replot();
}

void RealtimePlotWidget::appendLoad(double timeSec, double percent)
{
    if (!m_plot || m_plot->graphCount() < 2)
        return;
    m_plot->graph(1)->addData(timeSec, percent);
    m_plot->replot();
}
