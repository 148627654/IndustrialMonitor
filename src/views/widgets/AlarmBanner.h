#ifndef ALARMBANNER_H
#define ALARMBANNER_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include "AlarmDef.h"

/**
 * @brief 全局悬浮声光告警条 (Industrial HUD Banner)
 * @details 非模态顶置通知条：Critical 严重报警时借助 QPropertyAnimation 平滑下弹展开，
 *          支持多重告警队列滚动与消音确认闭环。折叠态高度为 0，不遮挡任何下层交互。
 * @author Yuquan Guo
 * @date 2026-10
 */
class AlarmBanner : public QFrame
{
    Q_OBJECT
public:
    explicit AlarmBanner(QWidget *parent = nullptr);

    // 弹出展示严重告警 (自动计算未确认严重告警队列位置 i/n)
    void showAlert(const AlarmEvent &event);
    // 平滑折叠收起
    void dismissAlert();

protected:
    // 手工接管背景绘制：全局 QSS 会遮蔽局部背景色声明，必须绕行 (同 RowGlowDelegate 策略)
    void paintEvent(QPaintEvent *event) override;
    // polish/style 变更后强制收回背景绘制权 (详见 cpp 注释)
    void changeEvent(QEvent *event) override;
    // 固定期望高度：让最大高度约束动画能真正顶到 BANNER_HEIGHT
    QSize sizeHint() const override;

private slots:
    void onAckClicked();

private:
    QLabel *m_lblIcon = nullptr;
    QLabel *m_lblText = nullptr;
    QPushButton *m_btnAck = nullptr;
    QPropertyAnimation *m_anim = nullptr;

    AlarmEvent m_currentEvent;
    const int BANNER_HEIGHT = 44;
};

#endif // ALARMBANNER_H
