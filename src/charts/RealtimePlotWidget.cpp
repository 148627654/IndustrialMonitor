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
    m_tempGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    m_tempGraph->setName("模具温度");
    m_tempGraph->setPen(QPen(QColor("#FF9800"), 2));               // 高信噪比亮橙
    m_tempGraph->setBrush(QColor(255, 152, 0, 20));                // 下方半透明渐变面积光晕

    // ==================== 曲线 1：主轴负荷 (右 Y 轴) ====================
    m_plot->yAxis2->setVisible(true);                            // 显式激活右轴
    m_cpuGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis2);
    m_cpuGraph->setName("主轴负荷");
    m_cpuGraph->setPen(QPen(QColor("#00E5FF"), 2));               // 工业科技青蓝

    // ==================== 双 Y 轴独立物理量纲 ====================
    m_plot->yAxis->setLabel("模具温度 (℃)");
    m_plot->yAxis->setRange(0.0, 120.0);                         // 固定量程

    m_plot->yAxis2->setLabel("主轴负荷 (%)");
    m_plot->yAxis2->setRange(0.0, 100.0);                        // 固定量程

    // ==================== X 轴：纳秒级绝对物理时间轴 ====================
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm:ss");   // 工业标准时分秒刻度
    dateTicker->setTickCount(6);                 // 建议绘制 6 个主时间刻度
    m_plot->xAxis->setTicker(dateTicker);
    m_plot->xAxis->setLabel("物理时间 (hh:mm:ss)");

    // 初始视口锚定到当前时刻的前 5 分钟窗口 (毫秒级浮点时间戳)
    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_plot->xAxis->setRange(now - m_windowSpanSeconds, now);

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

void RealtimePlotWidget::addDataPoint(double timestamp, double temperature, double cpuUsage)
{
    if (!m_tempGraph || !m_cpuGraph)
        return;

    m_tempGraph->addData(timestamp, temperature);
    m_cpuGraph->addData(timestamp, cpuUsage);

    // 环形切片淘汰：物理摘除 5 分钟窗口之前的过期历史点 (O(log N) 区间擦除，进一个出一个)
    const double expireTime = timestamp - m_windowSpanSeconds;
    m_tempGraph->data()->removeBefore(expireTime);
    m_cpuGraph->data()->removeBefore(expireTime);

    // 边界防御：窗口尚未填满时，左边界钳制在首个数据点处，杜绝左侧倒悬大片空白
    double lowerBound = expireTime;
    if (!m_tempGraph->data()->isEmpty())
        lowerBound = qMax(expireTime, m_tempGraph->data()->constBegin()->key);

    // 首点防御：保证视口至少 1 秒跨度，避免首个数据点产生零宽退化视口
    const double upperBound = timestamp;
    if (upperBound - lowerBound < 1.0)
        lowerBound = upperBound - 1.0;

    // 视口平滑向右推进
    m_plot->xAxis->setRange(lowerBound, upperBound);
    m_plot->replot();
}

void RealtimePlotWidget::clearData()
{
    if (m_tempGraph)
        m_tempGraph->data()->clear();
    if (m_cpuGraph)
        m_cpuGraph->data()->clear();
    double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_plot->xAxis->setRange(now - m_windowSpanSeconds, now);
    m_plot->replot();
}

void RealtimePlotWidget::setTimeWindowSpan(int seconds)
{
    m_windowSpanSeconds = qMax(10, seconds); // 至少保留 10 秒

    // 若缓冲区已有数据，立即按新窗口执行一次淘汰与视口重排 (不必等待下一个采样点)
    if (m_tempGraph && !m_tempGraph->data()->isEmpty()) {
        const double latest = m_tempGraph->data()->at(m_tempGraph->data()->size() - 1)->key;
        const double expire = latest - m_windowSpanSeconds;
        m_tempGraph->data()->removeBefore(expire);
        m_cpuGraph->data()->removeBefore(expire);
        const double lower = qMax(expire, m_tempGraph->data()->constBegin()->key);
        m_plot->xAxis->setRange(lower, latest);
        m_plot->replot();
    }
}
