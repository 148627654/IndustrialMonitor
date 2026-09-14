#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initLayout();
    initNavigation();
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
    statusBar()->showMessage("系统就绪 | 核心监控引擎已启动", 0);
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
