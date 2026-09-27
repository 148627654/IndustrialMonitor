#include "mainwindow.h"
#include "progressbardelegate.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include <QDateTime>
#include <QStyle>
#include <QRandomGenerator>
#include "../common/DeviceDef.h"
#include "statusdelegate.h"
#include "adddevicedialog.h"

#include <QElapsedTimer>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_navigationGroup(nullptr)
    , m_labelOperator(nullptr)
    , m_labelLinked(nullptr)
    , m_labelClock(nullptr)
    , m_labelLinkStatus(nullptr)
    , m_clockTimer(new QTimer(this))
    , m_dataPumpTimer(new QTimer(this))
    , m_deviceModel(new DeviceTableModel(this))
    , m_proxyModel(new DeviceProxyModel(this))
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

    initMonitorPageUI();

}

void MainWindow::initNavigation()
{
    if (!m_navigationGroup) {
        m_navigationGroup = new QButtonGroup(this);
    }
    QList<QPushButton*> navBtns =
    {
        ui->btnMonitor,
        ui->btnTrend,
        ui->btnAlarm,
        ui->btnSetting
    };

    for(auto i:navBtns)
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
    m_labelLinkStatus = new QLabel("通信链路: 正常", linkWidget);
    m_labelLinkStatus->setStyleSheet("color: #DCDCDC; font-size: 12px;");

    linkLayout->addWidget(m_labelLinked);
    linkLayout->addWidget(m_labelLinkStatus);

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

    connect(ui->btnRefresh, &QPushButton::clicked, this, [this]() {
        QList<DeviceInfo> currentList = m_deviceModel->deviceList();
        if (currentList.isEmpty()) return;

        for (auto &dev : currentList) {
            dev.status = static_cast<DeviceStatus>(QRandomGenerator::global()->bounded(4));
            dev.temperature = 30.0 + QRandomGenerator::global()->bounded(55);
            dev.cpuUsage = 10.0 + QRandomGenerator::global()->bounded(85);
            dev.memoryUsage = 20.0 + QRandomGenerator::global()->bounded(75);
        }

        m_deviceModel->setDeviceList(currentList);
        statusBar()->showMessage(QString("🔄 产线设备数据就地刷新成功 (当前共 %1 台设备在线)").arg(currentList.size()), 2500);
    });

    // 2. 交互式弹窗新增设备
    connect(ui->btnAddDevice, &QPushButton::clicked, this, [this]() {
        AddDeviceDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            DeviceInfo newDev = dlg.getDeviceInfo();
            m_deviceModel->addDevice(newDev);

            // 视口自动平滑下潜到底部
            ui->tableViewDevices->scrollToBottom();

            statusBar()->showMessage(QString("➕ 成功接入新设备工作站: %1 (%2)").arg(newDev.name, newDev.id), 3000);
        }
    });

    // 绑定 Ctrl+B 触发 10,000 条压测 (测试完按一下即可)
    QShortcut *benchmarkShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(benchmarkShortcut, &QShortcut::activated, this, [this]() {
        runStressBenchmark(10000); // 注入 10000 条！
    });
}

void MainWindow::setLinkStatus(LinkStatus status, const QString &text)
{
    m_labelLinkStatus->setText(text);

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

    m_proxyModel->setSourceModel(m_deviceModel);
    ui->tableViewDevices->setModel(m_proxyModel);

    // 开启表头排序
    ui->tableViewDevices->setSortingEnabled(true);
    ui->tableViewDevices->sortByColumn(DeviceTableModel::Col_Id,Qt::AscendingOrder);


    ui->tableViewDevices->setItemDelegateForColumn(
        DeviceTableModel::Col_Status,
        new StatusDelegate(this)
        );

    connect(ui->editSearchDevice,&QLineEdit::textChanged,m_proxyModel,&DeviceProxyModel::setSearchKeyword);

    ProgressBarDelegate *progressDelegate = new ProgressBarDelegate(this);
    ui->tableViewDevices->setItemDelegateForColumn(
        DeviceTableModel::Col_Cpu,
        progressDelegate
        );

    ui->tableViewDevices->setItemDelegateForColumn(
        DeviceTableModel::Col_Memory,
        progressDelegate
        );

    // 3. 启动实时数据泵定时器 (每 1000 毫秒跳动一次)
    connect(m_dataPumpTimer, &QTimer::timeout, this, [this]() {
        m_deviceModel->updateRandomData();
    });
    m_dataPumpTimer->start(1000);

    // 3. 注入批量工业假数据测试吞吐
    QList<DeviceInfo> testList;
    for (int i = 1; i <= 20; ++i) {
        DeviceInfo dev;
        dev.id = QString("DEV-%1").arg(1000 + i);
        dev.name = QString("%1号注塑工作站").arg(i);
        dev.ip = QString("192.168.1.%1").arg(100 + i);
        dev.port = 502;
        dev.status = static_cast<DeviceStatus>(i % 4);
        dev.temperature = 35.0 - (i * 2.3);
        dev.cpuUsage = 15.0 + (i * 3.5);
        dev.memoryUsage = 20.0 + (i * 2.8);
        testList.append(dev);
    }

    m_deviceModel->setDeviceList(testList);


}



void MainWindow::runStressBenchmark(int count)
{
    statusBar()->showMessage(QString("⚡ 正在生成并灌入 %1 条工业海量数据...").arg(count));
    qApp->processEvents(); // 强制先刷新界面显示提示

    QElapsedTimer timer;
    timer.start(); // ⏱️ 开始高精度计时

    // 1. 内存批量生成 10,000 条结构体
    QList<DeviceInfo> benchmarkList;
    benchmarkList.reserve(count); // 预分配内存，杜绝频繁扩容

    for (int i = 1; i <= count; ++i) {
        DeviceInfo dev;
        dev.id = QString("DEV-%1").arg(10000 + i);
        dev.name = QString("%1号数控加工中心").arg(i);
        dev.ip = QString("192.168.%1.%2").arg((i / 254) % 255).arg(i % 254 + 1);
        dev.port = 502;
        dev.status = static_cast<DeviceStatus>(i % 4);
        dev.temperature = 20.0 + (i % 700) / 10.0;
        dev.cpuUsage = 5.0 + (i % 900) / 10.0;
        dev.memoryUsage = 10.0 + (i % 850) / 10.0;
        benchmarkList.append(dev);
    }

    qint64 genTime = timer.elapsed(); // 数据生成耗时

    // 2. 注入模型并驱动视口重绘
    m_deviceModel->setDeviceList(benchmarkList);
    qint64 totalTime = timer.elapsed(); // 总加载耗时

    // 3. 统计结果输出至状态栏和控制台
    QString resultMsg = QString("🚀 压测完成: 成功加载 %1 条设备 | 生成耗时: %2ms | 视口渲染总耗时: %3ms")
                            .arg(count).arg(genTime).arg(totalTime);

    statusBar()->showMessage(resultMsg, 5000);
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

