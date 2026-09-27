#include "adddevicedialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>

AddDeviceDialog::AddDeviceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("新增工业监控设备");
    setFixedSize(360, 290);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    m_editId = new QLineEdit("DEV-", this);
    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText("例如: 5号注塑机");

    m_editIp = new QLineEdit("192.168.1.", this);
    m_spinPort = new QSpinBox(this);
    m_spinPort->setRange(1, 65535);
    m_spinPort->setValue(502);

    m_comboStatus = new QComboBox(this);
    m_comboStatus->addItem("正常", static_cast<int>(DeviceStatus::Normal));
    m_comboStatus->addItem("预警", static_cast<int>(DeviceStatus::Warning));
    m_comboStatus->addItem("故障", static_cast<int>(DeviceStatus::Fault));
    m_comboStatus->addItem("离线", static_cast<int>(DeviceStatus::Offline));
    m_comboStatus->setCurrentIndex(0); // 默认正常

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setSpacing(12);
    formLayout->addRow("设备编号:", m_editId);
    formLayout->addRow("设备名称:", m_editName);
    formLayout->addRow("IP 地址:", m_editIp);
    formLayout->addRow("通信端口:", m_spinPort);
    formLayout->addRow("初始状态:", m_comboStatus);

    QPushButton *btnOk = new QPushButton("确定添加", this);
    QPushButton *btnCancel = new QPushButton("取消", this);
    btnOk->setFixedHeight(32);
    btnCancel->setFixedHeight(32);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnOk);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(25, 20, 25, 15);
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, [this]() {
        if (m_editId->text().trimmed() == "DEV-" || m_editName->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "输入错误", "设备编号和名称不能为空！");
            return;
        }
        accept();
    });
}

DeviceInfo AddDeviceDialog::getDeviceInfo() const
{
    DeviceInfo dev;
    dev.id = m_editId->text().trimmed();
    dev.name = m_editName->text().trimmed();
    dev.ip = m_editIp->text().trimmed();
    dev.port = m_spinPort->value();
    dev.status = static_cast<DeviceStatus>(m_comboStatus->currentData().toInt());
    dev.temperature = 0.0;
    dev.cpuUsage = 0.0;
    dev.memoryUsage = 0.0;
    return dev;
}