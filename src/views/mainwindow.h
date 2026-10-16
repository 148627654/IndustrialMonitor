#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QLabel>
#include <QEvent>
#include <QComboBox>
#include "../models/devicetablemodel.h"
#include "../models/deviceproxymodel.h"
class QTableWidget;
class AlarmBanner;
class RealtimePlotWidget;
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
    void initMonitorPageUI();
private:
    Ui::MainWindow *ui;
    QButtonGroup *m_navigationGroup;
    QMap<QAbstractButton*, QWidget*> m_pageMap;

    //状态栏的封装
    QLabel *m_labelOperator;
    QLabel *m_labelLinked;
    QLabel *m_labelClock;
    QLabel *m_labelLinkStatus;
    QTimer *m_clockTimer;
    QTimer *m_dataPumpTimer;

    DeviceTableModel *m_deviceModel;
    DeviceProxyModel *m_proxyModel;

    void setLinkStatus(LinkStatus status, const QString &text);
    void initViewtable();
    // void runStressBenchmark(int count = 10000); // 声明压测函数

    void loadDevicesFromDatabase();             // 从数据库调用数据

    void initAlarmBanner();                     // 顶部全局悬浮告警条装配与全域联动
    void initAlarmLogPage();                    // 第 3 页报警日志流水表装配
    void initPlotPage();                        // 第 2 页实时曲线双 Y 轴波形组件装配

private slots:
    void refreshAlarmLog();                     // 与告警中心历史缓冲区全量同步刷新

private slots:
    void updateSystemTime();
    void on_btnRefresh_clicked();
protected:
    // 监听事件（处理点击空白处）
    bool eventFilter(QObject *watched, QEvent *event) override;
private slots:
    void onCustomContextMenuRequested(const QPoint &pos);

private:
    // 抽取的原子操作 (供双击、工具栏按钮、右键菜单多路共用)
    void editDeviceAtRow(int sourceRow);
    void deleteDeviceAtRow(int sourceRow);

    void updateFilterSummary();
    QComboBox *m_comboStatusFilter;
    QLabel    *m_lblFilterSummary;

    AlarmBanner *m_alarmBanner;                 // 全局悬浮告警条 (HUD)
    QTableWidget *m_alarmTable;                 // 第 3 页报警日志流水表
    RealtimePlotWidget *m_realtimePlot;         // 第 2 页实时曲线双 Y 轴波形组件

    // QWidget interface
protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
