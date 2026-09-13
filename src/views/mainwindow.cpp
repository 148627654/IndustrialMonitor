#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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
    ui->stackedWidget->setCurrentWidget(ui->page01Monitor);
}

MainWindow::~MainWindow()
{
    delete ui;
}
