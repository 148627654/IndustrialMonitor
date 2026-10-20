#include "PlotTracer.h"

#include <QDateTime>

PlotTracer::PlotTracer(QCustomPlot *plot, QCPGraph *tempGraph, QCPGraph *cpuGraph, QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_tempGraph(tempGraph)
    , m_cpuGraph(cpuGraph)
{
    // ==================== 1. 垂直准星线 (高反差淡灰虚线，贯穿整个坐标区) ====================
    m_vLine = new QCPItemStraightLine(m_plot);
    m_vLine->setLayer("overlay");
    m_vLine->setPen(QPen(QColor(220, 220, 220, 160), 1, Qt::DashLine));
    m_vLine->point1->setCoords(0, 0);
    m_vLine->point2->setCoords(0, 1);

    // ==================== 2. 温度磁吸高光圆点 (亮橙实心 + 纯白描边) ====================
    m_tempDot = new QCPItemTracer(m_plot);
    m_tempDot->setLayer("overlay");
    m_tempDot->setStyle(QCPItemTracer::tsCircle);
    m_tempDot->setSize(8);
    m_tempDot->setPen(QPen(Qt::white, 1.5));
    m_tempDot->setBrush(QColor("#FF9800"));

    // ==================== 3. 负荷磁吸高光圆点 (科技青蓝实心 + 纯白描边) ====================
    m_cpuDot = new QCPItemTracer(m_plot);
    m_cpuDot->setLayer("overlay");
    m_cpuDot->setStyle(QCPItemTracer::tsCircle);
    m_cpuDot->setSize(8);
    m_cpuDot->setPen(QPen(Qt::white, 1.5));
    m_cpuDot->setBrush(QColor("#00E5FF"));

    // ==================== 4. HUD 半透明暗黑悬浮读数卡片 ====================
    m_tooltip = new QCPItemText(m_plot);
    m_tooltip->setLayer("overlay");
    m_tooltip->setColor(QColor("#EEEEEE"));
    m_tooltip->setPen(QPen(QColor("#555555"), 1));
    m_tooltip->setBrush(QBrush(QColor(25, 25, 25, 220))); // 深黑半透明微光
    m_tooltip->setPadding(QMargins(8, 6, 8, 6));
    m_tooltip->setFont(QFont("Segoe UI", 9, QFont::Bold));

    setVisible(false);
}

PlotTracer::~PlotTracer() = default; // QCustomPlot 析构时连带释放绑定的图元

void PlotTracer::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    m_vLine->setVisible(visible);
    m_tempDot->setVisible(visible);
    m_cpuDot->setVisible(visible);
    m_tooltip->setVisible(visible);
    if (m_plot)
        m_plot->layer("overlay")->replot();
}

QString PlotTracer::tooltipText() const
{
    return m_tooltip ? m_tooltip->text() : QString();
}

bool PlotTracer::tooltipAnchoredRight() const
{
    return m_anchoredRight;
}

void PlotTracer::updatePosition(const QPoint &pixelPos)
{
    // ==================== 严密空数据与越界防御 ====================
    if (!m_plot || !m_tempGraph || m_tempGraph->dataCount() == 0
        || !m_plot->axisRect()->rect().contains(pixelPos)) {
        if (m_visible) {
            m_hasSnap = false;
            setVisible(false);
        }
        return;
    }

    // ==================== 1. 鼠标像素坐标 → 绝对时间戳 ====================
    const double mouseCoordX = m_plot->xAxis->pixelToCoord(pixelPos.x());

    // ==================== 2. 核心：O(log N) 二分最近邻磁吸 ====================
    // 时序数据严格单调递增，利用容器底层红黑树二分检索首个 key >= mouseCoordX 的点，
    // 再与其前驱点比对绝对距离，取最近者 —— 绝不悬浮在采样点之间的虚空
    auto it = m_tempGraph->data()->findBegin(mouseCoordX, false);
    if (it == m_tempGraph->data()->constEnd()) {
        it = m_tempGraph->data()->constEnd() - 1; // 光标越过最右点，吸附最新点
    } else if (it != m_tempGraph->data()->constBegin()) {
        auto prev = it;
        --prev;
        if (qAbs(mouseCoordX - prev->key) < qAbs(it->key - mouseCoordX))
            it = prev;
    }

    const double snappedX = it->key;
    const double tempVal  = it->value;

    // ==================== 3. 右轴负荷通道同刻对齐 (两图同源时戳，精确相等) ====================
    double cpuVal = 0.0;
    if (m_cpuGraph && m_cpuGraph->dataCount() > 0) {
        auto cpuIt = m_cpuGraph->data()->findBegin(snappedX, false);
        if (cpuIt != m_cpuGraph->data()->constEnd() && qFuzzyCompare(cpuIt->key, snappedX))
            cpuVal = cpuIt->value;
        else if (cpuIt != m_cpuGraph->data()->constBegin()) {
            --cpuIt;
            cpuVal = cpuIt->value; // 兜底：取最接近的历史读数
        }
    }

    // ==================== 4. 游标图元磁吸物理就位 ====================
    m_vLine->point1->setCoords(snappedX, m_plot->yAxis->range().lower);
    m_vLine->point2->setCoords(snappedX, m_plot->yAxis->range().upper);
    m_tempDot->position->setCoords(snappedX, tempVal);
    m_cpuDot->position->setCoords(snappedX, cpuVal);

    // ==================== 5. HUD 浮窗排版与智能边缘避让翻转 ====================
    const QString timeStr = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(snappedX * 1000.0))
                                .toString("hh:mm:ss.zzz");
    m_tooltip->setText(QString("⏱ %1\n🌡 模具温度: %2 ℃\n⚡ 主轴负荷: %3 %")
                           .arg(timeStr)
                           .arg(tempVal, 5, 'f', 1)
                           .arg(cpuVal, 5, 'f', 1));

    // 水平翻转：光标越过坐标区中心 (视口 70% 语义) 时，浮窗锚定游标左侧展开
    m_anchoredRight = (pixelPos.x() > m_plot->axisRect()->center().x());
    // 垂直避让：光标位于下半区时，浮窗锚定在下方向上展开，杜绝底部截断
    const bool anchorTop = (pixelPos.y() <= m_plot->axisRect()->center().y());

    if (m_anchoredRight)
        m_tooltip->setPositionAlignment(anchorTop ? (Qt::AlignTop | Qt::AlignRight)
                                                  : (Qt::AlignBottom | Qt::AlignRight));
    else
        m_tooltip->setPositionAlignment(anchorTop ? (Qt::AlignTop | Qt::AlignLeft)
                                                  : (Qt::AlignBottom | Qt::AlignLeft));

    m_tooltip->position->setCoords(snappedX, anchorTop ? m_plot->yAxis->range().upper * 0.95
                                                       : m_plot->yAxis->range().lower * 1.05);

    // ==================== 6. 记录吸附结果 ====================
    m_hasSnap = true;
    m_snappedKey = snappedX;
    m_snappedTemp = tempVal;
    m_snappedCpu = cpuVal;

    if (!m_visible)
        setVisible(true);
    else
        m_plot->layer("overlay")->replot(); // 仅重绘顶层游标图层，微秒级轻量刷新
}
