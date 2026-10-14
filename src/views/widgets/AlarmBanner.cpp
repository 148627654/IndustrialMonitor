#include "AlarmBanner.h"

#include "alarmmanager.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>

namespace {

// 从告警中心提取未确认的 Critical 严重告警 (按时间 新→旧 排列)
QList<AlarmEvent> unackedCriticalAlarms()
{
    QList<AlarmEvent> result;
    const QList<AlarmEvent> active = AlarmManager::instance().getActiveAlarms();
    for (const AlarmEvent &ev : active) {
        if (ev.level == AlarmLevel::Critical)
            result.prepend(ev);   // history 旧→新，prepend 反转得到 新→旧
    }
    return result;
}

} // namespace

AlarmBanner::AlarmBanner(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("alarmBanner");
    // paintEvent 会完整重绘整个矩形，声明不透明绘制
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    // 默认高度收缩为 0 (不可见且不占空间，由动画驱动展开)
    setFixedHeight(0);

    // 深红工业警示 QSS 主题
    // 注意：QLabel 必须显式 transparent —— 全局 QWidget 底色规则会给 QLabel 灌入不透明背景，
    // 导致横跨告警条中部的文字标签把 paintEvent 绘制的绯红底色整块盖住 (排查良久)
    setStyleSheet(
        "QLabel { color: #FFFFFF; font-weight: bold; font-size: 13px; background: transparent; }"
        "QPushButton {"
        "   background-color: #FF4D4F; color: #FFFFFF; border: none; border-radius: 4px;"
        "   padding: 5px 14px; font-weight: bold; font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #FF7875; }"
        "QPushButton:pressed { background-color: #D9363E; }"
    );

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);

    m_lblIcon = new QLabel("🚨", this);
    m_lblIcon->setStyleSheet("font-size: 18px; background: transparent;");

    m_lblText = new QLabel(this);
    m_lblText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_lblText->setStyleSheet("background: transparent;");

    m_btnAck = new QPushButton("🔕 消音确认", this);
    m_btnAck->setCursor(Qt::PointingHandCursor);
    connect(m_btnAck, &QPushButton::clicked, this, &AlarmBanner::onAckClicked);

    layout->addWidget(m_lblIcon);
    layout->addWidget(m_lblText);
    layout->addWidget(m_btnAck);

    // 属性动画引擎 (作用于 maximumHeight，最大高度约束法杜绝布局抖动)
    m_anim = new QPropertyAnimation(this, "maximumHeight", this);
    m_anim->setDuration(260);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

QSize AlarmBanner::sizeHint() const
{
    // 固定期望高度：布局按 max(sizeHint, maximumHeight 动画值) 裁决实际高度，
    // 保证展开动画能完整顶到 BANNER_HEIGHT (否则只到子控件自然高度 ~30px 就停)
    return QSize(600, BANNER_HEIGHT);
}

void AlarmBanner::changeEvent(QEvent *event)
{
    // 关键防御：全局 QSS 的 QWidget 底色规则会在 polish 阶段强制开启 WA_StyledBackground，
    // 样式系统随后会以不透明底色覆盖本控件背景，导致 paintEvent 的绘制被抹除。
    // polish 的时机晚于构造函数甚至 showEvent，必须在 Polish/StyleChange 事件中
    // (样式系统完成属性注入之后) 立即收回背景绘制权。
    if (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange)
        setAttribute(Qt::WA_StyledBackground, false);
    QFrame::changeEvent(event);
}

void AlarmBanner::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    // 高饱和度绯红警示底 + 警示描边 (手工绘制，绕开全局 QSS 背景覆盖)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor("#F56C6C"), 1));
    p.setBrush(QColor("#7A1D1D"));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
}

void AlarmBanner::showAlert(const AlarmEvent &event)
{
    m_currentEvent = event;

    // 多重告警队列角标：计算当前告警在未确认严重告警队列中的位置 i/n
    const QList<AlarmEvent> queue = unackedCriticalAlarms();
    const int total = queue.size();
    int position = 1;
    for (int i = 0; i < queue.size(); ++i) {
        if (queue.at(i).alarmId == event.alarmId) {
            position = i + 1;
            break;
        }
    }

    m_lblText->setText(QString("[严重报警 %1/%2] 设备 [%3] %4")
                           .arg(position)
                           .arg(total)
                           .arg(event.deviceId)
                           .arg(event.description));

    // 防动画重入撕裂：先停止旧动画，以当前高度为新起点平滑续播
    m_anim->stop();
    m_anim->setStartValue(height());
    m_anim->setEndValue(BANNER_HEIGHT);
    m_anim->start();
}

void AlarmBanner::dismissAlert()
{
    m_anim->stop();
    m_anim->setStartValue(height());
    m_anim->setEndValue(0);
    m_anim->setEasingCurve(QEasingCurve::InCubic);   // 收起快速回弹归位
    m_anim->start();
}

void AlarmBanner::onAckClicked()
{
    if (m_currentEvent.alarmId.isEmpty())
        return;

    // 1. 消音闭环：确认当前告警，联动告警中心生命周期流转
    AlarmManager::instance().acknowledgeAlarm(m_currentEvent.alarmId);

    // 2. 队列滚动：若仍有未确认严重告警，自动滚动展示下一条 (最新优先)
    const QList<AlarmEvent> queue = unackedCriticalAlarms();
    if (!queue.isEmpty()) {
        showAlert(queue.first());
        return;
    }

    // 3. 全部确认完毕，平滑收起
    m_currentEvent = AlarmEvent();
    dismissAlert();
}
