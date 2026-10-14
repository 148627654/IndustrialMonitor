#include "views/AddDeviceDialog.h"
#include "common/ValidatorUtils.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStyle>
#include <QPushButton>

AddDeviceDialog::AddDeviceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("新增工业监控设备");
    setFixedSize(380, 300);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    // 1. 控件初始化与校验器注入
    m_editId = new QLineEdit(this);
    m_editId->setPlaceholderText("例如: DEV-1021");
    m_editId->setValidator(ValidatorUtils::createDeviceIdValidator(this)); // 物理拦截非法编号

    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText("例如: 5号高速注塑机");

    m_editIp = new QLineEdit(this);
    m_editIp->setPlaceholderText("例如: 192.168.1.125");
    m_editIp->setValidator(ValidatorUtils::createIpValidator(this)); // 物理拦截非法 IP

    m_spinPort = new QSpinBox(this);
    m_spinPort->setRange(1, 65535);
    m_spinPort->setValue(502);

    m_comboStatus = new QComboBox(this);
    m_comboStatus->addItem("正常", static_cast<int>(DeviceStatus::Normal));
    m_comboStatus->addItem("预警", static_cast<int>(DeviceStatus::Warning));
    m_comboStatus->addItem("故障", static_cast<int>(DeviceStatus::Fault));
    m_comboStatus->addItem("离线", static_cast<int>(DeviceStatus::Offline));
    m_comboStatus->setCurrentIndex(0);

    // 2. 布局搭建
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setSpacing(12);
    formLayout->addRow("设备编号:", m_editId);
    formLayout->addRow("设备名称:", m_editName);
    formLayout->addRow("通信 IP:", m_editIp);
    formLayout->addRow("通信端口:", m_spinPort);
    formLayout->addRow("初始状态:", m_comboStatus);

    m_btnOk = new QPushButton("确定添加", this);
    m_btnCancel = new QPushButton("取消", this);
    m_btnOk->setFixedHeight(32);
    m_btnCancel->setFixedHeight(32);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnCancel);
    btnLayout->addWidget(m_btnOk);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(25, 20, 25, 15);
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // 3. 信号槽绑定
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);

    // 核心联动：输入变动即时重新评估表单
    connect(m_editId,   &QLineEdit::textChanged, this, &AddDeviceDialog::validateForm);
    connect(m_editName, &QLineEdit::textChanged, this, &AddDeviceDialog::validateForm);
    connect(m_editIp,   &QLineEdit::textChanged, this, &AddDeviceDialog::validateForm);

    // 初始执行一次侦测（按钮默认置灰）
    validateForm();
}

void AddDeviceDialog::validateForm()
{
    bool idOk   = ValidatorUtils::isValidDeviceId(m_editId->text());
    bool nameOk = !m_editName->text().trimmed().isEmpty();
    bool ipOk   = ValidatorUtils::isValidIp(m_editIp->text());

    // 动态外边框警示反馈
    updateInputVisual(m_editId, idOk || m_editId->text().isEmpty());
    updateInputVisual(m_editIp, ipOk || m_editIp->text().isEmpty());

    // 只有全部合规，提交按钮才允许点亮激活
    bool allValid = idOk && nameOk && ipOk;
    m_btnOk->setEnabled(allValid);
}

void AddDeviceDialog::updateInputVisual(QLineEdit *edit, bool valid)
{
    // 利用动态属性配合 QSS 高亮黄边
    edit->setProperty("invalid", !valid);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
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