#ifndef REALTIMEPLOTWIDGET_H
#define REALTIMEPLOTWIDGET_H

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

    // 追加温度采样点 (自动挂载左轴温度曲线)
    void appendTemperature(double timeSec, double celsius);
    // 追加负荷采样点 (自动挂载右轴负荷曲线)
    void appendLoad(double timeSec, double percent);

private:
    void setupPlot();       // 双轴拓扑与曲线装配 (配置期严禁反复 replot)
    void setupDarkTheme();  // 工业暗黑 SCADA 主题注入

    QCustomPlot *m_plot = nullptr;
};

#endif // REALTIMEPLOTWIDGET_H
