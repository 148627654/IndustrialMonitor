#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include <QDateTime>
#include <QStyle>
#include "../common/DeviceDef.h"
#include <QStandardItemModel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_labelOperator(nullptr)
    ,m_labelLinked(nullptr)
    ,m_labelClock(nullptr)
    ,m_labellLinkStatus(nullptr)
    ,m_clockTimer(new QTimer(this))
{
    ui->setupUi(this);
    initLayout();
    initNavigation();
    initStatusBar();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initLayout()
{
    setWindowTitle("工业设备智能监控看板 - V1.0");
    resize(1280, 800);
    setMinimumSize(1280, 800);

    //设置边框
    if(ui->centralWidget->layout())
    {
        ui->centralWidget->layout()->setContentsMargins(0,0,0,0);           //边框设置为0
        ui->centralWidget->layout()->setSpacing(0);
    }


    ui->sidebarWidget->setFixedWidth(200);
    // statusBar()->showMessage("系统就绪 | 核心监控引擎已启动", 0);

    initMonitorPageUI();

}

void MainWindow::initNavigation()
{
    if (!m_navigationGroup) {
        m_navigationGroup = new QButtonGroup(this);
    }
    QList<QPushButton*> m_navBtn=
    {
        ui->btnMonitor,
        ui->btnTrend,
        ui->btnAlarm,
        ui->btnSetting
    };

    for(auto i:m_navBtn)
    {
        if (!i) {
            qWarning() << "⚠ 发现空指针按钮！请检查 Qt Designer 中按钮的 objectName 是否拼写一致！";
            continue;
        }
        i->setCheckable(true);
        // 1. 高度固定为 46px (45~48px 之间均可)
        i->setFixedHeight(46);

        // 2. 水平策略设为 Expanding (自动撑满侧边栏的 200px 宽度)
        i->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_navigationGroup->addButton(i);
    }
    m_navigationGroup->setExclusive(true);

    //构建映射
    m_pageMap.clear();
    m_pageMap.insert(ui->btnMonitor,ui->page01Monitor);
    m_pageMap.insert(ui->btnTrend,ui->page02Trend);
    m_pageMap.insert(ui->btnAlarm,ui->page03Alarm);
    m_pageMap.insert(ui->btnSetting,ui->page04Setting);

    connect(m_navigationGroup,&QButtonGroup::buttonClicked,this,[this](QAbstractButton *btn)
    {
        if(m_pageMap.contains(btn))
            ui->stackedWidget->setCurrentWidget(m_pageMap[btn]);
    });
    ui->btnMonitor->setChecked(true);
    ui->stackedWidget->setCurrentWidget(ui->page01Monitor);
}

void MainWindow::initStatusBar()
{
    m_labelOperator = new QLabel("👤 操作员: admin [系统管理员]", this);
    m_labelOperator->setStyleSheet("color: #DCDCDC; padding-left: 10px; font-weight: 500;");

    // 2. 通信链路状态组合部件 (居中常驻)
    QWidget *linkWidget = new QWidget(this);
    QHBoxLayout *linkLayout = new QHBoxLayout(linkWidget);
    linkLayout->setContentsMargins(15, 0, 15, 0);
    linkLayout->setSpacing(6);

    m_labelLinked = new QLabel(linkWidget);
    m_labelLinked->setObjectName("linkLed");
    m_labelLinked->setFixedSize(10, 10);
    m_labellLinkStatus = new QLabel("通信链路: 正常", linkWidget);
    m_labellLinkStatus->setStyleSheet("color: #DCDCDC; font-size: 12px;");

    linkLayout->addWidget(m_labelLinked);
    linkLayout->addWidget(m_labellLinkStatus);

    //时间
    m_labelClock = new QLabel(this);
    m_labelClock->setStyleSheet("color: #FFFFFF; font-family: 'Consolas', monospace; font-size: 13px; font-weight: bold; padding-right: 12px;");
    m_labelClock->setFixedWidth(190);
    m_labelClock->setAlignment(Qt::AlignRight | Qt::AlignVCenter);


    // 4. 挂载至底部状态栏（常驻模式，杜绝被 showMessage 冲刷）

    statusBar()->addWidget(m_labelOperator, 1);      // 权重 1，占据左侧全部弹性空间

    statusBar()->addPermanentWidget(linkWidget);   // 居右常驻
    statusBar()->addPermanentWidget(m_labelClock);    // 最右侧常驻

    // 5. 初始化指示灯与时钟
    setLinkStatus(LinkStatus::Normal, "通信链路: 在线 (10ms)");

    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateSystemTime);
    updateSystemTime();      // 立即刷新，消除首秒空白
    m_clockTimer->start(1000);
}

void MainWindow::initMonitorPageUI()
{
    if (ui->page01Monitor->layout()) {
        ui->page01Monitor->layout()->setContentsMargins(15,15,15,15);
        ui->page01Monitor->layout()->setSpacing(12);
    }
    initViewtable();
}

void MainWindow::setLinkStatus(LinkStatus status, const QString &text)
{
    m_labellLinkStatus->setText(text);

    QString statusVal = "normal";
    switch (status) {
    case LinkStatus::Normal:  statusVal = "normal"; break;
    case LinkStatus::Warning: statusVal = "warning"; break;
    case LinkStatus::Fault:   statusVal = "fault"; break;
    case LinkStatus::Offline: statusVal = "offline"; break;
    }

    m_labelLinked->setProperty("status", statusVal);
    m_labelLinked->style()->unpolish(m_labelLinked);
    m_labelLinked->style()->polish(m_labelLinked);
    m_labelLinked->update();
}

void MainWindow::initViewtable()
{
    ui->tableViewDevices->setSelectionBehavior(QAbstractItemView::SelectRows);          //整行选择
    ui->tableViewDevices->setSelectionMode(QAbstractItemView::SingleSelection);         //单行选择模式
    ui->tableViewDevices->setEditTriggers(QAbstractItemView::NoEditTriggers);           //只读禁止直接编辑
    ui->tableViewDevices->setAlternatingRowColors(true);                                //开启交替行底色
    ui->tableViewDevices->setFocusPolicy(Qt::NoFocus);                                  //关闭默认虚线焦点框
    ui->tableViewDevices->verticalHeader()->setVisible(false);                          //隐藏垂直行号表头
    ui->tableViewDevices->horizontalHeader()->setHighlightSections(false);              //高亮锁定关闭
    ui->tableViewDevices->horizontalHeader()->setStretchLastSection(true);              // 最后一列自适应撑满

    /*
        测试
    */
    QStandardItemModel *testModel = new QStandardItemModel(this);

    // 1. 设置表头列名
    QStringList headers = {
        "设备编号", "设备名称", "IP地址", "端口",
        "运行状态", "温度(℃)", "CPU占用", "内存占用"
    };

    // 2. 模拟 3 台工业设备假数据 (对应 DeviceInfo 结构)
    QList<QList<QString>> mockRows = {
        {"DEV-1001", "1号注塑机", "192.168.1.101", "502", "正常", "45.2", "28.5%", "42.1%"},
        {"DEV-1002", "2号压铸机", "192.168.1.102", "502", "预警", "78.6", "82.0%", "65.4%"},
        {"DEV-1003", "3号数控车床", "192.168.1.103", "502", "离线", "0.0",  "0.0%",  "0.0%"}
    };

    for (const auto &row : mockRows) {
        QList<QStandardItem*> items;
        for (const auto &text : row) {
            items.append(new QStandardItem(text));
        }

        testModel->appendRow(items);
    }
    DeviceInfo my;
    my.name="nullptr";
    my.id="0";
    my.ip="0.0.0.0";

    QList<QStandardItem*> mylist;
    /*
    QString id;                 //设备编号
    QString name;               //设备名称
    QString ip;                 //IP地址
    int port=7988;                   //端口号
    DeviceStatus status=DeviceStatus::Offline;        //运行状态
    double temperature=0.0;         //温度
    double cpuUsage=0.0;            //CPU占用率
    double memoryUsage=0.0;         //内存占用率
    */

    mylist.append(new QStandardItem(my.id));
    mylist.append(new QStandardItem(my.name));
    mylist.append(new QStandardItem(my.ip));
    mylist.append(new QStandardItem(my.port));
    mylist.append(new QStandardItem("在线"));
    mylist.append(new QStandardItem(my.temperature));
    mylist.append(new QStandardItem(my.cpuUsage));
    mylist.append(new QStandardItem(my.memoryUsage));
    testModel->appendRow(mylist);
    // 3. 将模型装配到 TableView 视口上
    ui->tableViewDevices->setModel(testModel);
}

void MainWindow::updateSystemTime()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    m_labelClock->setText(currentTime);
}
void MainWindow::on_btnRefresh_clicked()
{
    statusBar()->showMessage("系统就绪 | 核心监控引擎已启动", 3000);
}

