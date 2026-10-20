#include "RealtimePlotWidget.h"

#include "qcustomplot.h"
#include "PlotTracer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QMouseEvent>

RealtimePlotWidget::RealtimePlotWidget(QWidget *parent)
    : QWidget(parent)
{
    // 1. 顶部工业控制工具条
    initUi();

    // 2. 装配双轴拓扑与曲线
    setupPlot();

    // 3. 注入工业暗黑主题
    setupDarkTheme();

    // 4. 全部配置完毕，统一只触发一次重绘 (杜绝配置期白斑闪烁)
    m_plot->replot();
}

RealtimePlotWidget::~RealtimePlotWidget() = default;

void RealtimePlotWidget::initUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // ==================== 顶部工业控制工具条 (固定高度宿主，杜绝与图表抢占纵向空间) ====================
    QWidget *toolbarHost = new QWidget(this);
    toolbarHost->setFixedHeight(36);

    auto *toolbar = new QHBoxLayout(toolbarHost);
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(12);

    QLabel *lblDev = new QLabel("监控设备:", toolbarHost);
    lblDev->setStyleSheet("color: #FFFFFF; font-weight: bold;");
    lblDev->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_comboDevice = new QComboBox(toolbarHost);
    m_comboDevice->setMinimumWidth(220);
    m_comboDevice->setStyleSheet(
        "QComboBox { background-color: #2D343C; color: #FFFFFF;"
        " border: 1px solid #444444; border-radius: 4px; padding: 4px; }");
    connect(m_comboDevice, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RealtimePlotWidget::onDeviceChanged);

    m_chkTemp = new QCheckBox("模具温度 (℃)", toolbarHost);
    m_chkTemp->setChecked(true);
    m_chkTemp->setStyleSheet("color: #FF9800; font-weight: bold;");
    connect(m_chkTemp, &QCheckBox::toggled, this, &RealtimePlotWidget::onChannelVisibilityChanged);

    m_chkCpu = new QCheckBox("主轴负荷 (%)", toolbarHost);
    m_chkCpu->setChecked(true);
    m_chkCpu->setStyleSheet("color: #00E5FF; font-weight: bold;");
    connect(m_chkCpu, &QCheckBox::toggled, this, &RealtimePlotWidget::onChannelVisibilityChanged);

    m_chkAutoScroll = new QCheckBox("自动跟随", toolbarHost);
    m_chkAutoScroll->setChecked(true);
    m_chkAutoScroll->setStyleSheet("color: #67C23A;");
    connect(m_chkAutoScroll, &QCheckBox::toggled, this, &RealtimePlotWidget::onAutoScrollToggled);

    m_lblReadout = new QLabel("等待数据...", toolbarHost);
    m_lblReadout->setStyleSheet("color: #CCCCCC; font-style: italic;");
    m_lblReadout->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    toolbar->addWidget(lblDev);
    toolbar->addWidget(m_comboDevice);
    toolbar->addWidget(m_chkTemp);
    toolbar->addWidget(m_chkCpu);
    toolbar->addWidget(m_chkAutoScroll);
    toolbar->addStretch(1);
    toolbar->addWidget(m_lblReadout);

    mainLayout->addWidget(toolbarHost);

    // ==================== 下方波形大屏 ====================
    m_plot = new QCustomPlot(this);
    m_plot->setObjectName("realtimePlot");
    mainLayout->addWidget(m_plot);
}

void RealtimePlotWidget::setupPlot()
{
    // ==================== 曲线 0：模具温度 (左 Y 轴) ====================
    m_tempGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    m_tempGraph->setName("模具温度");
    m_tempGraph->setPen(QPen(QColor("#FF9800"), 2));               // 高信噪比亮橙
    m_tempGraph->setBrush(QColor(255, 152, 0, 20));                // 下方半透明渐变面积光晕

    // ==================== 曲线 1：主轴负荷 (右 Y 轴) ====================
    m_plot->yAxis2->setVisible(true);                              // 显式激活右轴
    m_cpuGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis2);
    m_cpuGraph->setName("主轴负荷");
    m_cpuGraph->setPen(QPen(QColor("#00E5FF"), 2));                // 工业科技青蓝

    // ==================== 双 Y 轴独立物理量纲 ====================
    m_plot->yAxis->setLabel("模具温度 (℃)");
    m_plot->yAxis->setRange(0.0, 120.0);                           // 固定量程

    m_plot->yAxis2->setLabel("主轴负荷 (%)");
    m_plot->yAxis2->setRange(0.0, 100.0);                          // 固定量程

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

    // ==================== 十字交叉追踪游标探针 ====================
    m_tracer = new PlotTracer(m_plot, m_tempGraph, m_cpuGraph, this);

    // 鼠标移动 → 二分磁吸计算 (updatePosition 内部仅在有效吸附时触发 overlay 轻量重绘)
    connect(m_plot, &QCustomPlot::mouseMove, this, [this](QMouseEvent *event) {
        if (m_tracer)
            m_tracer->updatePosition(event->pos());
    });

    // 图表视口需要持续追踪鼠标，才能捕获滑入/滑出过渡
    m_plot->setMouseTracking(true);
    setMouseTracking(true);
}

void RealtimePlotWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 鼠标滑入工具条等图表外区域时，清爽隐藏整套游标
    if (m_tracer && !m_plot->rect().contains(event->pos()))
        m_tracer->setVisible(false);
    QWidget::mouseMoveEvent(event);
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

double RealtimePlotWidget::lowerBoundFor(double latestTimestamp) const
{
    // 滑动窗口左边界：默认 = 最新时刻 - 窗口跨度
    double lower = latestTimestamp - m_windowSpanSeconds;

    // 边界防御：窗口尚未填满时，钳制在首个数据点处，杜绝左侧倒悬大片空白
    if (m_tempGraph && !m_tempGraph->data()->isEmpty())
        lower = qMax(lower, m_tempGraph->data()->constBegin()->key);

    // 首点防御：保证视口至少 1 秒跨度，避免零宽退化视口
    if (latestTimestamp - lower < 1.0)
        lower = latestTimestamp - 1.0;

    return lower;
}

void RealtimePlotWidget::addDataPoint(double timestamp, double temperature, double cpuUsage, bool scrollViewport)
{
    if (!m_tempGraph || !m_cpuGraph)
        return;

    m_tempGraph->addData(timestamp, temperature);
    m_cpuGraph->addData(timestamp, cpuUsage);

    // 环形切片淘汰：物理摘除 5 分钟窗口之前的过期历史点 (O(log N) 区间擦除，进一个出一个)
    const double expireTime = timestamp - m_windowSpanSeconds;
    m_tempGraph->data()->removeBefore(expireTime);
    m_cpuGraph->data()->removeBefore(expireTime);

    // 仅当自动跟随开启时才推进视口 (排障定格模式下数据照常追加与淘汰，画面保持冻结)
    if (scrollViewport)
        m_plot->xAxis->setRange(lowerBoundFor(timestamp), timestamp);

    m_plot->replot();
}

void RealtimePlotWidget::clearData()
{
    if (m_tempGraph)
        m_tempGraph->data()->clear();
    if (m_cpuGraph)
        m_cpuGraph->data()->clear();
    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
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
        m_plot->xAxis->setRange(lowerBoundFor(latest), latest);
        m_plot->replot();
    }
}

void RealtimePlotWidget::setDeviceList(const QList<DeviceInfo> &devices)
{
    // 阻断信号防抖：批量 addItem 会触发多次 currentIndexChanged，必须整体静默重构
    m_comboDevice->blockSignals(true);
    m_comboDevice->clear();
    for (const DeviceInfo &dev : devices) {
        m_comboDevice->addItem(QString("[%1] %2").arg(dev.id, dev.name), dev.id);
    }
    m_comboDevice->blockSignals(false);

    if (!devices.isEmpty()) {
        m_selectedDeviceId = devices.first().id;
        clearData(); // 重置时间轴基线
    }
}

void RealtimePlotWidget::onDeviceChanged(int index)
{
    if (index < 0)
        return;
    const QString newId = m_comboDevice->itemData(index).toString();
    if (newId == m_selectedDeviceId)
        return;

    m_selectedDeviceId = newId;
    clearData(); // 铁律：强制清空旧机台波形缓存，严防跨设备飞线撕裂
    m_lblReadout->setText("已切换至 " + m_selectedDeviceId + "，等待下一帧...");
}

void RealtimePlotWidget::onChannelVisibilityChanged()
{
    const bool showTemp = m_chkTemp->isChecked();
    const bool showCpu  = m_chkCpu->isChecked();

    // 光栅化直接跳过隐藏 Graph，大幅降低重绘多边形计算量
    m_tempGraph->setVisible(showTemp);
    m_cpuGraph->setVisible(showCpu);

    // 对应 Y 轴随通道一并显隐 (刻度/网格/标签)，最大化重绘开销节省
    m_plot->yAxis->setVisible(showTemp);
    m_plot->yAxis2->setVisible(showCpu);

    m_plot->replot();
}

void RealtimePlotWidget::onAutoScrollToggled(bool checked)
{
    m_autoScroll = checked;

    // 重新开启跟随时，视口秒级瞬移追平最新时刻
    if (m_autoScroll && m_tempGraph && !m_tempGraph->data()->isEmpty()) {
        const double latest = m_tempGraph->data()->at(m_tempGraph->data()->size() - 1)->key;
        m_plot->xAxis->setRange(lowerBoundFor(latest), latest);
        m_plot->replot();
    }
}

void RealtimePlotWidget::onTelemetryBatchReceived(const QList<TelemetryFrame> &batch)
{
    if (m_selectedDeviceId.isEmpty() || batch.isEmpty())
        return;

    // 解复用路由中枢：遍历全厂空间快照，仅精准抓取当前选中机台的帧，静默丢弃其余设备
    for (const TelemetryFrame &frame : batch) {
        if (frame.deviceId != m_selectedDeviceId)
            continue;

        const double t = frame.timestamp.toMSecsSinceEpoch() / 1000.0;
        addDataPoint(t, frame.temperature, frame.cpuUsage, m_autoScroll);

        // 刷新顶部实时读数标签
        m_lblReadout->setText(QString("温度: %1 ℃ | 负荷: %2 %")
                                  .arg(frame.temperature, 5, 'f', 1)
                                  .arg(frame.cpuUsage, 5, 'f', 1));
        break; // 命中目标帧后立即退出
    }
}
