#ifndef PLOTTRACER_H
#define PLOTTRACER_H

#include <QObject>
#include <QPoint>
#include "qcustomplot.h"

/**
 * @brief 工业示波器十字交叉追踪游标与最近邻磁吸浮窗 (Crosshair Tracer & Snapping Tooltip)
 * @details 由垂直时间虚线、双通道磁吸高光圆点与暗黑 HUD 读数卡片构成。
 *          利用时序数据单调递增特性，通过 QCustomPlot 底层 O(log N) 二分检索
 *          将游标强力吸附在最近的真实采样点上，杜绝悬空虚拟读数。
 *          全部图元挂载于 overlay 图层，重绘仅限顶层，微秒级轻量刷新。
 * @author Yuquan Guo
 * @date 2026-10
 */
class PlotTracer : public QObject
{
public:
    explicit PlotTracer(QCustomPlot *plot, QCPGraph *tempGraph, QCPGraph *cpuGraph, QObject *parent = nullptr);
    ~PlotTracer() override;

    // 响应鼠标像素移动并执行二分吸附计算 (像素位于坐标区外时自动隐藏)
    void updatePosition(const QPoint &pixelPos);
    // 显隐控制 (整体开关整套游标图元，状态无变化时不触发重绘)
    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    // ==================== 最近一次吸附结果 (供单测与外部读数) ====================
    bool hasSnappedPoint() const { return m_hasSnap; }
    double snappedKey() const { return m_snappedKey; }      // 吸附点绝对时间戳 (秒)
    double snappedTemp() const { return m_snappedTemp; }    // 左轴温度读数
    double snappedCpu() const { return m_snappedCpu; }      // 右轴负荷读数
    QString tooltipText() const;                            // HUD 浮窗当前文本
    bool tooltipAnchoredRight() const;                      // 浮窗是否锚定在游标右侧(左翻转)

private:
    QCustomPlot *m_plot = nullptr;
    QCPGraph *m_tempGraph = nullptr;
    QCPGraph *m_cpuGraph = nullptr;

    // 游标视觉图元 (全部挂载 overlay 顶层)
    QCPItemStraightLine *m_vLine = nullptr;   // 垂直时间虚线
    QCPItemTracer *m_tempDot = nullptr;       // 温度磁吸高光圆点
    QCPItemTracer *m_cpuDot = nullptr;        // 负荷磁吸高光圆点
    QCPItemText *m_tooltip = nullptr;         // 暗黑 HUD 读数卡片

    bool m_visible{false};
    bool m_hasSnap{false};
    bool m_anchoredRight{false};
    double m_snappedKey{0.0};
    double m_snappedTemp{0.0};
    double m_snappedCpu{0.0};
};

#endif // PLOTTRACER_H
