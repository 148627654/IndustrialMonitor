#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QLabel>
// #include <QAbstractButton>
QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

enum class LinkStatus {
    Normal,   // 正常在线 (绿)
    Warning,  // 延迟/告警 (黄)
    Fault,    // 故障/断开 (红)
    Offline   // 离线休眠 (灰)
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void initLayout();
    void initNavigation();
    void initStatusBar();
private:
    Ui::MainWindow *ui;
    QButtonGroup *m_navigationGroup;
    QMap<QAbstractButton*, QWidget*> m_pageMap;

    //状态栏的封装
    QLabel *m_labelOperator;
    QLabel *m_labelLinked;
    QLabel *m_labelClock;
    QLabel *m_labellLinkStatus;
    QTimer *m_clockTimer;
    void setLinkStatus(LinkStatus status, const QString &text);
private slots:
    void updateSystemTime();
    void on_btnRefresh_clicked();
};
#endif // MAINWINDOW_H
