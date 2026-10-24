#ifndef REALTIMEPLOTWIDGET_H
#define REALTIMEPLOTWIDGET_H

#include "qcustomplot.h"
#include <QWidget>
#include <QList>
#include <QPushButton>
#include "DeviceDef.h"
#include "TelemetryDef.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QResizeEvent;
class PlotTracer;

/**
 * @brief 工业级双 Y 轴实时波形组件 (Dual-Axis SCADA Plot + 解复用路由中枢)
 * @details 内部托管 QCustomPlot 引擎，构建左右双独立物理量纲坐标体系：
 *          左轴 (yAxis) 全权绑定模具温度 (0~120 ℃，亮橙警示色)；
 *          右轴 (yAxis2) 全权绑定主轴负荷 (0~100 %，科技青蓝冷色)。
 *          顶部集成工业控制工具条 (设备选择器 / 通道显隐 / 自动跟随 / 实时读数)；
 *          作为全厂批量遥测总线的解复用器 (Demux)，仅提取当前选中设备帧注入
 *          5 分钟滑动窗口，静默丢弃其余设备帧。
 *          交互层：X 轴独占滚轮无级缩放 + 拖拽平移 (双 Y 轴刚性锁止)，用户主动漫游
 *          即刻解除自动跟随并浮现右下角"⟲ 回到当前时刻"HUD 胶囊按钮，一键瞬移追平；
 *          渲染层：分层解耦 (overlay 游标独立重绘) + rpQueuedReplot 排队合并 + 仅曲线抗锯齿，
 *          支撑 50Hz 遥测冲击下 60 FPS 满帧交互。
 * @author Yuquan Guo
 * @date 2026-10
 */
class RealtimePlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RealtimePlotWidget(QWidget *parent = nullptr);
    ~RealtimePlotWidget() override;

    // 暴露底层绘图引擎 (供遥测数据追加、缩放与导出交互)
    QCustomPlot *plot() const { return m_plot; }
    // 十字追踪游标探针 (供单测与外部读取吸附结果)
    PlotTracer *tracer() const { return m_tracer; }

    // 载入设备列表供下拉框选择 (与表格台账同源)
    void setDeviceList(const QList<DeviceInfo> &devices);
    // 当前选中的目标设备编号
    QString currentDeviceId() const { return m_selectedDeviceId; }

    // 唯一数据入口：向两条曲线追加一个采样时刻的物理量
    // (scrollViewport=false 时仅追加与淘汰数据，视口保持定格供排障审阅)
    void addDataPoint(double timestamp, double temperature, double cpuUsage, bool scrollViewport = true);
    // 清空历史数据并重置时间轴基线
    void clearData();
    // 配置时间窗口大小 (默认 300 秒)
    void setTimeWindowSpan(int seconds);
    int timeWindowSpan() const { return m_windowSpanSeconds; }

public slots:
    // 消费全厂批量遥测数据包并提取当前设备追加波形 (解复用路由中枢)
    void onTelemetryBatchReceived(const QList<TelemetryFrame> &batch);

private slots:
    void onDeviceChanged(int index);              // 设备热切换 + 缓冲区冷迁移
    void onChannelVisibilityChanged();            // 通道显隐 + Y 轴联动
    void onAutoScrollToggled(bool checked);       // 自动跟随定格 / 追平
    void onPlotUserInteraction();                 // 用户主动漫游 → 解除自动跟随 + 浮现恢复按钮
    void onResumeScrollClicked();                 // 一键瞬移追平最新时刻 + 收起恢复按钮

protected:
    // 鼠标滑入工具条等图表外区域时隐藏游标
    void mouseMoveEvent(QMouseEvent *event) override;
    // 悬浮 HUD 按钮绝对定位：钉在视口右下角 (避开底部坐标轴)
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUi();                                // 顶部工业控制工具条
    void setupPlot();                             // 双轴拓扑与曲线装配
    void setupDarkTheme();                        // 工业暗黑 SCADA 主题
    double lowerBoundFor(double latestTimestamp) const; // 滑动窗口左边界 (含防御钳制)

    // 顶部控制条组件
    QComboBox *m_comboDevice = nullptr;
    QCheckBox *m_chkTemp = nullptr;
    QCheckBox *m_chkCpu = nullptr;
    QCheckBox *m_chkAutoScroll = nullptr;
    QLabel *m_lblReadout = nullptr;

    // 绘图核心
    QCustomPlot *m_plot = nullptr;
    QCPGraph *m_tempGraph = nullptr;
    QCPGraph *m_cpuGraph = nullptr;
    PlotTracer *m_tracer = nullptr;             // 十字交叉追踪游标探针

    // 右下角悬浮 HUD 胶囊按钮 (独立于布局，resizeEvent 中绝对定位)
    QPushButton *m_btnResumeScroll = nullptr;

    QString m_selectedDeviceId;
    bool m_autoScroll{true};          // 自动滚动跟随开关
    int m_windowSpanSeconds{300};     // 滑动窗口宽度 (秒)
};

#endif // REALTIMEPLOTWIDGET_H
