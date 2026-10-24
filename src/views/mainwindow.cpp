#include "mainwindow.h"
#include "progressbardelegate.h"
#include "rowglowdelegate.h"
#include "ui_mainwindow.h"
#include "../common/DeviceDef.h"
#include "../dao/devicedao.h"
#include "views/editdevicedialog.h"
#include <QTimer>
#include <QDateTime>
#include <QStyle>
#include <QRandomGenerator>
#include <alarmmanager.h>
#include "../common/DeviceDef.h"
#include "statusdelegate.h"
#include "adddevicedialog.h"
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QShortcut>
#include <QGuiApplication>
#include <QClipboard>
#include <acquisitioncontroller.h>
#include "widgets/AlarmBanner.h"
#include "RealtimePlotWidget.h"
#include <QTableWidget>
#include <QHeaderView>


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
    , m_alarmBanner(nullptr)
    , m_alarmTable(nullptr)
    , m_realtimePlot(nullptr)
{
    ui->setupUi(this);
    initLayout();
    initAlarmBanner();
    initAlarmLogPage();
    initPlotPage();
    initNavigation();
    initStatusBar();

    // ==================== V3 采集引擎并线生产：全链路真实数据源点火 ====================
    // 引擎启动后，节流总线 (throttledBatchReady) 自动驱动三路消费端：
    //   ① 设备台账表格 (updateTelemetryBatch)  ② ISA-18.2 告警状态机 (evaluateBatch)
    //   ③ 实时曲线解复用路由 (onTelemetryBatchReceived)
    AcquisitionController &controller = AcquisitionController::instance();
    if (controller.init())
        controller.start(); // 采集周期读取 t_sys_config (缺省 1000ms / 1Hz)
    else
        qCritical() << "[MainWindow] Acquisition engine initialization failed!";
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
    // m_labelOperator = new QLabel(this);
    // QPixmap pix(":/icons/admin.png");
    // pix = pix.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // m_labelOperator->setPixmap(pix);
    // m_labelOperator->setText("操作员: admin [系统管理员]");
    // m_labelOperator->setAlignment(Qt::AlignVCenter);
    // m_labelOperator->setIndent(6); // 图片与文字之间的间距


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

    m_comboStatusFilter = new QComboBox(this);
    m_comboStatusFilter->addItem("全部状态", -1);
    m_comboStatusFilter->addItem("🟢 正常", static_cast<int>(DeviceStatus::Normal));
    m_comboStatusFilter->addItem("🟡 预警", static_cast<int>(DeviceStatus::Warning));
    m_comboStatusFilter->addItem("🔴 故障", static_cast<int>(DeviceStatus::Fault));
    m_comboStatusFilter->addItem("⚪ 离线", static_cast<int>(DeviceStatus::Offline));
    m_comboStatusFilter->setFixedWidth(110);

    m_lblFilterSummary = new QLabel(this);
    m_lblFilterSummary->setObjectName("lblFilterSummary");

    ui->horizontalLayout_2->addWidget(m_comboStatusFilter);

    ui->horizontalLayout_2->addWidget(m_lblFilterSummary);

    connect(m_comboStatusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        int status = m_comboStatusFilter->currentData().toInt();
        m_proxyModel->setStatusFilter(status);
        updateFilterSummary();
    });

    // 5. 搜索框文字变化联动指标
    connect(ui->editSearchDevice, &QLineEdit::textChanged, this, [this]() {
        updateFilterSummary();
    });

    // 初始执行一次指标计算
    updateFilterSummary();

    // 更新
    connect(ui->btnRefresh, &QPushButton::clicked, this,&MainWindow::on_btnRefresh_clicked);

    // 2. 交互式弹窗新增设备
    connect(ui->btnAddDevice, &QPushButton::clicked, this, [this]() {
        AddDeviceDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            DeviceInfo newDev = dlg.getDeviceInfo();

            // 检查主键是否冲突
            if(DeviceDao::existsById(newDev.id))
            {
                QMessageBox::warning(this,"添加失败",QString("设备编号[%1]已经在数据库中存在，严谨重复添加!").arg(newDev.id));
                return;
            }

            // 先写入数据库
            if(!DeviceDao::insertDevice(newDev))
            {
                QMessageBox::critical(this,"入库失败","写入数据库失败，请检查连接状态!");
                return;
            }

            m_deviceModel->addDevice(newDev);

            // 视口自动平滑下潜到底部
            ui->tableViewDevices->scrollToBottom();

            statusBar()->showMessage(QString("➕ 成功接入新设备工作站: %1 (%2)").arg(newDev.name, newDev.id), 3000);
        }
    });

    // 删除
    auto updateDeleteBtnState=[this](){
        bool hasSel=ui->tableViewDevices->selectionModel()&&
                      ui->tableViewDevices->selectionModel()->hasSelection();
        ui->btnDeleteDevice->setEnabled(hasSel);
    };

    connect(ui->tableViewDevices->selectionModel(),&QItemSelectionModel::selectionChanged,this,updateDeleteBtnState);
    updateDeleteBtnState();         // 灰色
    connect(ui->btnDeleteDevice,&QPushButton::clicked,this,[this](){
        QModelIndex proxyIndex=ui->tableViewDevices->currentIndex();
        if(!proxyIndex.isValid())return;

        QModelIndex sourceIndex=m_proxyModel->mapToSource(proxyIndex);
        int sourceRow=sourceIndex.row();
        DeviceInfo targetDev=m_deviceModel->getDevice(sourceRow);

        // 弹窗 二次提醒
        QMessageBox msgBox(QMessageBox::Warning,"注销确认",
                           QString("确认从系统中永久注销设备[%1 - %2]吗?\n此操作将同步销毁本地数据库，不可撤销")
                               .arg(targetDev.id,targetDev.name),
                           QMessageBox::Yes | QMessageBox::No,this);
        msgBox.button(QMessageBox::Yes)->setText("确认注销");
        msgBox.button(QMessageBox::No)->setText("取消放弃");
        msgBox.setDefaultButton(QMessageBox::No);
        if(msgBox.exec()!=QMessageBox::Yes)
            return;
        if(!DeviceDao::deleteDeviceById(targetDev.id))
        {
            QMessageBox::critical(this, "删除失败", "从数据库删除记录失败，请检查数据库连接状态！");
            return;
        }
        m_deviceModel->removeDevice(sourceRow);
        // 状态栏
        statusBar()->showMessage(QString("🗑 设备 [%1 - %2] 已成功物理注销并从台账移除")
                                     .arg(targetDev.id, targetDev.name), 3000);
    });
    // 绑定 Ctrl+B 触发 10,000 条压测 (测试完按一下即可)
    // QShortcut *benchmarkShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    // connect(benchmarkShortcut, &QShortcut::activated, this, [this]() {
    //     runStressBenchmark(10000); // 注入 10000 条！
    // });
    connect(&AcquisitionController::instance(), &AcquisitionController::throttledBatchReady,
            this, [this](const QList<TelemetryFrame> &batch) {
                AlarmManager::instance().evaluateBatch(batch);
                m_deviceModel->updateTelemetryBatch(batch);
            });
}

void MainWindow::initAlarmBanner()
{
    m_alarmBanner = new AlarmBanner(this);

    // 重构中央布局：原"侧边栏 + 页面栈"横向布局整体下沉，顶部插入全局悬浮告警条
    if (QLayout *oldLayout = ui->centralWidget->layout()) {
        QWidget *sidebar        = ui->sidebarWidget;
        QWidget *rightContainer = ui->stackedWidget->parentWidget();

        oldLayout->removeWidget(sidebar);
        oldLayout->removeWidget(rightContainer);
        delete oldLayout;

        QHBoxLayout *contentRow = new QHBoxLayout;
        contentRow->setContentsMargins(0, 0, 0, 0);
        contentRow->setSpacing(0);
        contentRow->addWidget(sidebar);
        contentRow->addWidget(rightContainer, 1);

        QVBoxLayout *rootLayout = new QVBoxLayout(ui->centralWidget);
        rootLayout->setContentsMargins(8, 6, 8, 0);
        rootLayout->setSpacing(6);
        rootLayout->addWidget(m_alarmBanner);
        rootLayout->addLayout(contentRow, 1);
    }

    // 全域联动：无论操作员停留在哪个子页面，Critical 严重报警均无条件顶置弹出
    connect(&AlarmManager::instance(), &AlarmManager::alarmTriggered,
            this, [this](const AlarmEvent &event) {
        if (event.level == AlarmLevel::Critical) {
            m_alarmBanner->showAlert(event);
        }
    });
}

void MainWindow::initAlarmLogPage()
{
    QVBoxLayout *layout = new QVBoxLayout(ui->page03Alarm);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);

    QLabel *title = new QLabel("🚨 报警日志流水 (Alarm History)", ui->page03Alarm);
    title->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold;");
    layout->addWidget(title);

    m_alarmTable = new QTableWidget(0, 7, ui->page03Alarm);
    m_alarmTable->setObjectName("alarmLogTable");
    m_alarmTable->setHorizontalHeaderLabels({"流水号", "触发时间", "设备编号", "级别", "详细描述", "触发值", "确认状态"});
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);          // 只读流水
    m_alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_alarmTable->verticalHeader()->setVisible(false);
    m_alarmTable->horizontalHeader()->setHighlightSections(false);
    m_alarmTable->horizontalHeader()->setStretchLastSection(true);
    m_alarmTable->setAlternatingRowColors(true);
    layout->addWidget(m_alarmTable);

    // 新告警产生或被消音确认时，与主控中枢环形缓冲区全量同步
    connect(&AlarmManager::instance(), &AlarmManager::alarmTriggered,
            this, &MainWindow::refreshAlarmLog);
    connect(&AlarmManager::instance(), &AlarmManager::alarmAcknowledged,
            this, &MainWindow::refreshAlarmLog);
    refreshAlarmLog();
}

void MainWindow::refreshAlarmLog()
{
    const QList<AlarmEvent> history = AlarmManager::instance().getHistoricalAlarms();
    m_alarmTable->setRowCount(history.size());

    // 时间倒序：最新告警置顶
    for (int i = 0; i < history.size(); ++i) {
        const AlarmEvent &ev = history.at(history.size() - 1 - i);

        QString levelText;
        QColor  levelColor;
        switch (ev.level) {
        case AlarmLevel::Critical:
            levelText  = "严重报警";
            levelColor = QColor("#FF6B6B");   // 深红
            break;
        case AlarmLevel::Warning:
            levelText  = "预警";
            levelColor = QColor("#FFD600");   // 明黄
            break;
        default:
            levelText  = "信息";
            levelColor = QColor("#9E9E9E");
            break;
        }

        const QStringList cols = {
            ev.alarmId,
            ev.timestamp.toString("yyyy-MM-dd HH:mm:ss"),
            ev.deviceId,
            levelText,
            ev.description,
            QString::number(ev.triggerValue, 'f', 1) + " ℃",
            ev.acknowledged ? "已确认" : "未确认"
        };

        for (int c = 0; c < cols.size(); ++c) {
            auto *item = new QTableWidgetItem(cols.at(c));
            item->setForeground(levelColor);
            m_alarmTable->setItem(i, c, item);
        }
    }
}

void MainWindow::initPlotPage()
{
    // 第 2 页"实时曲线"：挂载双 Y 轴工业波形组件，自适应撑满整个工作区
    QVBoxLayout *layout = new QVBoxLayout(ui->page02Trend);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(0);

    m_realtimePlot = new RealtimePlotWidget(ui->page02Trend);
    layout->addWidget(m_realtimePlot);

    // 注入全厂设备台账 (复用已载入的模型数据，与表格台账 100% 同源一致)
    m_realtimePlot->setDeviceList(m_deviceModel->deviceList());

    // 大动脉贯通：节流遥测总线 → 图表解复用路由中枢 (仅提取当前选中设备帧)
    connect(&AcquisitionController::instance(), &AcquisitionController::throttledBatchReady,
            m_realtimePlot, &RealtimePlotWidget::onTelemetryBatchReceived);
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


    // 整行告警微光委托：铺设至全部普通列 (状态灯/进度条列内部亦已手工铺设底层微光)
    ui->tableViewDevices->setItemDelegate(new RowGlowDelegate(this));

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

    // 3. 实时数据源已由采集引擎节流总线统一接管 (见构造函数末尾 controller.start())
    //    原随机仿真泵正式退役 —— 表格不再由本地随机数驱动，改由真实采集帧刷新
    // connect(m_dataPumpTimer, &QTimer::timeout, this, [this]() {
    //     m_deviceModel->updateRandomData();
    // });
    // m_dataPumpTimer->start(1000);
    ui->tableViewDevices->viewport()->installEventFilter(this);
    loadDevicesFromDatabase();

    // 开启表格视口自定义右键
    ui->tableViewDevices->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableViewDevices,&QTableView::customContextMenuRequested,this,&MainWindow::onCustomContextMenuRequested);

    // 双击表格行进行复用编辑逻辑
    connect(ui->tableViewDevices,&QTableView::doubleClicked,this,[this](const QModelIndex &proxyIndex){
        if(!proxyIndex.isValid())
            return;
        int sourceIndex=m_proxyModel->mapToSource(proxyIndex).row();
        editDeviceAtRow(sourceIndex);
    });
}

void MainWindow::loadDevicesFromDatabase()
{
    // 调用DAO层从SQLite中提取真实的台账数据
    QList<DeviceInfo> dbDevices=DeviceDao::queryAllDevices();

    m_deviceModel->setDeviceList(dbDevices);

    statusBar()->showMessage(QString("✅ 成功从 SQLite 数据库载入 %1 台设备台账").arg(dbDevices.size()), 3000);
}



// void MainWindow::runStressBenchmark(int count)
// {
//     statusBar()->showMessage(QString("⚡ 正在生成并灌入 %1 条工业海量数据...").arg(count));
//     qApp->processEvents(); // 强制先刷新界面显示提示

//     QElapsedTimer timer;
//     timer.start(); // ⏱️ 开始高精度计时

//     // 1. 内存批量生成 10,000 条结构体
//     QList<DeviceInfo> benchmarkList;
//     benchmarkList.reserve(count); // 预分配内存，杜绝频繁扩容

//     for (int i = 1; i <= count; ++i) {
//         DeviceInfo dev;
//         dev.id = QString("DEV-%1").arg(10000 + i);
//         dev.name = QString("%1号数控加工中心").arg(i);
//         dev.ip = QString("192.168.%1.%2").arg((i / 254) % 255).arg(i % 254 + 1);
//         dev.port = 502;
//         dev.status = static_cast<DeviceStatus>(i % 4);
//         dev.temperature = 20.0 + (i % 700) / 10.0;
//         dev.cpuUsage = 5.0 + (i % 900) / 10.0;
//         dev.memoryUsage = 10.0 + (i % 850) / 10.0;
//         benchmarkList.append(dev);
//     }

//     qint64 genTime = timer.elapsed(); // 数据生成耗时

//     // 2. 注入模型并驱动视口重绘
//     m_deviceModel->setDeviceList(benchmarkList);
//     qint64 totalTime = timer.elapsed(); // 总加载耗时

//     // 3. 统计结果输出至状态栏和控制台
//     QString resultMsg = QString("🚀 压测完成: 成功加载 %1 条设备 | 生成耗时: %2ms | 视口渲染总耗时: %3ms")
//                             .arg(count).arg(genTime).arg(totalTime);

//     statusBar()->showMessage(resultMsg, 5000);
// }

void MainWindow::updateSystemTime()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    m_labelClock->setText(currentTime);
}
void MainWindow::on_btnRefresh_clicked()
{
    loadDevicesFromDatabase();
    statusBar()->showMessage("🔄 已完成本地 SQLite 数据库台账同步刷新", 2500);
}



bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 1. 判断事件是否来自于表格的视口，且是鼠标点击事件
    if (watched == ui->tableViewDevices->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        // 2. indexAt 检测鼠标点击的物理坐标下是否有具体的行
        QModelIndex clickedIndex = ui->tableViewDevices->indexAt(mouseEvent->pos());

        // 3. 如果 clickedIndex 是无效的 (说明点在了没有数据的空白区域)
        if (!clickedIndex.isValid()) {
            ui->tableViewDevices->clearSelection();              // 清除所有选中行
            ui->tableViewDevices->setCurrentIndex(QModelIndex()); // 清除当前焦点锚点
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex proxyIndex = ui->tableViewDevices->indexAt(pos);
    bool hasTarget = proxyIndex.isValid();

    int sourceRow = -1;
    DeviceInfo targetDev;

    if (hasTarget) {
        // 核心直觉交互：右键点击哪一行，光标和高亮就自动切到该行！
        ui->tableViewDevices->setCurrentIndex(proxyIndex);
        sourceRow = m_proxyModel->mapToSource(proxyIndex).row();
        targetDev = m_deviceModel->getDevice(sourceRow);
    }

    QMenu menu(this);
    QAction *actEdit      = new QAction(QIcon(":/icons/change.svg"),"修改设备参数", &menu);
    // D:\Cpp\Qt\IndustrialMonitor\IndustrialMonitor\res\icons\change.svg
    QAction *actDelete    = new QAction(QIcon(":/icons/delete.svg"),"注销当前设备", &menu);
    QAction *actReconnect = new QAction(QIcon(":/icons/refresh.svg"),"立即就地重连", &menu);
    QAction *actCopyAddr  = new QAction(QIcon(":/icons/copy.svg"),"复制通信地址", &menu);

    // 行级高阶动作：未击中行时智能置灰！
    actEdit->setEnabled(hasTarget);
    actDelete->setEnabled(hasTarget);
    actReconnect->setEnabled(hasTarget);
    actCopyAddr->setEnabled(hasTarget);

    menu.addAction(actEdit);
    menu.addAction(actReconnect);
    menu.addAction(actCopyAddr);
    menu.addSeparator();
    menu.addAction(actDelete);

    // 全局辅助动作 (任何时候都可用)
    menu.addSeparator();
    QAction *actAdd     = new QAction(QIcon(":/icons/add.svg"),"新增监控设备", &menu);
    QAction *actRefresh = new QAction(QIcon(":/icons/refresh.svg"),"同步本地台账", &menu);
    menu.addAction(actAdd);
    menu.addAction(actRefresh);

    // 3. 串联动作业务响应
    connect(actEdit, &QAction::triggered, this, [this, sourceRow]() {
        editDeviceAtRow(sourceRow);
    });

    connect(actDelete, &QAction::triggered, this, [this, sourceRow]() {
        deleteDeviceAtRow(sourceRow);
    });

    // 复制通信地址到系统剪贴板 (IP:Port)
    connect(actCopyAddr, &QAction::triggered, this, [this, targetDev]() {
        QString addr = QString("%1:%2").arg(targetDev.ip).arg(targetDev.port);
        QGuiApplication::clipboard()->setText(addr);
        statusBar()->showMessage(QString("📋 已复制通信地址到系统剪贴板: %1").arg(addr), 2500);
    });

    // 模拟工业快速就地重连
    connect(actReconnect, &QAction::triggered, this, [this, sourceRow, targetDev]() {
        DeviceInfo reconnectedDev = targetDev;
        reconnectedDev.status = DeviceStatus::Normal; // 快速复位为正常
        DeviceDao::updateDevice(reconnectedDev);
        m_deviceModel->updateDevice(sourceRow, reconnectedDev);
        statusBar()->showMessage(QString("🔄 设备 [%1] 通信握手重连成功，状态已恢复正常").arg(reconnectedDev.id), 2500);
    });

    connect(actAdd,     &QAction::triggered, ui->btnAddDevice,  &QPushButton::click);
    connect(actRefresh, &QAction::triggered, ui->btnRefresh,    &QPushButton::click);

    // 4. 转换物理坐标并在鼠标点击处弹出 (基于视口画布 mapToGlobal)
    menu.exec(ui->tableViewDevices->viewport()->mapToGlobal(pos));
}

void MainWindow::editDeviceAtRow(int sourceRow)
{
    if(sourceRow<0)return;
    DeviceInfo targetDev=m_deviceModel->getDevice(sourceRow);

    EditDeviceDialog dlg(targetDev,this);
    if(dlg.exec()==QDialog::Accepted)
    {
        DeviceInfo updateDev=dlg.getDeviceInfo();
        if (DeviceDao::updateDevice(updateDev)) {
            m_deviceModel->updateDevice(sourceRow, updateDev);
            statusBar()->showMessage(QString("✏ 设备 [%1] 参数已成功修改并持久化").arg(updateDev.id), 3000);
        } else {
            QMessageBox::critical(this, "更新失败", "更新本地数据库失败！");
        }
    }
}

void MainWindow::deleteDeviceAtRow(int sourceRow)
{
    if (sourceRow < 0) return;
    DeviceInfo targetDev = m_deviceModel->getDevice(sourceRow);

    QMessageBox msgBox(QMessageBox::Warning, "注销设备确认",
                       QString("确认从系统中永久注销设备 [%1 - %2] 吗？\n此操作将同步销毁本地数据库台账！")
                           .arg(targetDev.id, targetDev.name),
                       QMessageBox::Yes | QMessageBox::No, this);
    msgBox.button(QMessageBox::Yes)->setText("确认注销");
    msgBox.button(QMessageBox::No)->setText("取消放弃");
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        if (DeviceDao::deleteDeviceById(targetDev.id)) {
            m_deviceModel->removeDevice(sourceRow);
            statusBar()->showMessage(QString("🗑 设备 [%1] 已成功物理注销").arg(targetDev.id), 3000);
        } else {
            QMessageBox::critical(this, "删除失败", "从数据库删除记录失败！");
        }
    }
}

void MainWindow::updateFilterSummary()
{
    int total = m_deviceModel->rowCount(QModelIndex());
    int matched = m_proxyModel->rowCount();

    m_lblFilterSummary->setText(QString("📊 纳管总数: %1 台 | 匹配显示: %2 台")
                                    .arg(total).arg(matched));
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    qInfo()<<"[MainWindow] close event triggered. Performing pre-shutdown teardown...";

    AcquisitionController::instance().stop();
    qInfo()<<"[MainWindow] Teardown complete, Accepting close event. ";
    event->accept();
}

