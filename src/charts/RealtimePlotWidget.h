#ifndef REALTIMEPLOTWIDGET_H
#define REALTIMEPLOTWIDGET_H

#include "qcustomplot.h"
#include <QWidget>

class QCustomPlot;

/**
 * @brief 工业级双 Y 轴实时波形组件 (Dual-Axis SCADA Plot)
 * @details 内部托管 QCustomPlot 引擎，构建左右双独立物理量纲坐标体系：
 *          左轴 (yAxis) 全权绑定模具温度 (0~120 ℃，亮橙警示色)；
 *          右轴 (yAxis2) 全权绑定主轴负荷 (0~100 %，科技青蓝冷色)。
 *          整套画布注入工业暗黑主题 (低眩光 #1A1A1A 画布 + #121212 坐标区 + 微光点阵网格)。
 * @author Yuquan Guo
 * @date 2026-10
 */
class RealtimePlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RealtimePlotWidget(QWidget *parent = nullptr);
    ~RealtimePlotWidget() override;

    // 暴露底层绘图引擎 (供后续遥测数据追加、缩放与导出交互)
    QCustomPlot *plot() const { return m_plot; }

    // 唯一数据入口：向两条曲线追加一个采样时刻的物理量
    // (timestamp 采用含毫秒的 Unix 秒级浮点时间戳，内部自动执行 5 分钟环形淘汰与视口推进)
    void addDataPoint(double timestamp, double temperature, double cpuUsage);
    // 清空历史数据并重置
    void clearData();
    // 配置时间窗口大小 (默认 300 秒)
    void setTimeWindowSpan(int seconds);
    int timeWindowSpan() const { return m_windowSpanSeconds; }

private:
    void setupPlot();       // 双轴拓扑与曲线装配 (配置期严禁反复 replot)
    void setupDarkTheme();  // 工业暗黑 SCADA 主题注入

    QCustomPlot *m_plot = nullptr;
    QCPGraph *m_tempGraph=nullptr;
    QCPGraph *m_cpuGraph=nullptr;
    int m_windowSpanSeconds{300}; // 滑动窗口宽度 (秒)
};

#endif // REALTIMEPLOTWIDGET_H
