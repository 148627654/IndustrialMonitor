#include "editdevicedialog.h"
#include "../common/validatorutils.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>

EditDeviceDialog::EditDeviceDialog(const DeviceInfo &dev, QWidget *parent)
    :QDialog(parent),
    m_originalDev(dev)
{
    setWindowTitle("修改工业设备参数");
    setFixedSize(380,300);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    // 主键只允许只读，不能修改
    m_editId=new QLineEdit(dev.id,this);
    m_editId->setReadOnly(true);
    m_editId->setToolTip("资产编号作为唯一物理主键，严禁修改");

    // 字段回填与校验器挂钩
    m_editName =new QLineEdit(dev.name,this);
    m_editIp = new QLineEdit(dev.ip,this);
    m_editIp->setValidator(ValidatorUtils::createIpValidator(this));

    m_spinPort=new QSpinBox(this);
    m_spinPort->setRange(1,65535);
    m_spinPort->setValue(dev.port);

    m_comboStatus = new QComboBox(this);
    m_comboStatus->addItem("正常", static_cast<int>(DeviceStatus::Normal));
    m_comboStatus->addItem("预警", static_cast<int>(DeviceStatus::Warning));
    m_comboStatus->addItem("故障", static_cast<int>(DeviceStatus::Fault));
    m_comboStatus->addItem("离线", static_cast<int>(DeviceStatus::Offline));

    int statusIdx=m_comboStatus->findData(static_cast<int>(dev.status));
    if(statusIdx!=-1)
        m_comboStatus->setCurrentIndex(statusIdx);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setSpacing(12);
    formLayout->addRow("设备编号:", m_editId);
    formLayout->addRow("设备名称:", m_editName);
    formLayout->addRow("通信 IP:", m_editIp);
    formLayout->addRow("通信端口:", m_spinPort);
    formLayout->addRow("运行状态:", m_comboStatus);

    m_btnSave   = new QPushButton("保存修改", this);
    m_btnCancel = new QPushButton("取消", this);
    m_btnSave->setFixedHeight(32);
    m_btnCancel->setFixedHeight(32);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnCancel);
    btnLayout->addWidget(m_btnSave);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(25, 20, 25, 15);
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);
    // 4. 信号槽绑定
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnSave,   &QPushButton::clicked, this, &QDialog::accept);
    connect(m_editName, &QLineEdit::textChanged, this, &EditDeviceDialog::validateForm);
    connect(m_editIp,   &QLineEdit::textChanged, this, &EditDeviceDialog::validateForm);
    validateForm();
}

DeviceInfo EditDeviceDialog::getDeviceInfo() const
{
    DeviceInfo update=m_originalDev;
    update.name=m_editName->text().trimmed();
    update.ip=m_editIp->text().trimmed();
    update.port=m_spinPort->value();
    update.status=static_cast<DeviceStatus>(m_comboStatus->currentData().toInt());
    return update;
}

void EditDeviceDialog::updateInputVisual(QLineEdit *edit, bool valid)
{
    edit->setProperty("invalid",!valid);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
}

void EditDeviceDialog::validateForm()
{
    bool nameOK=!m_editName->text().trimmed().isEmpty();
    bool ipOK=ValidatorUtils::isValidIp(m_editIp->text());

    updateInputVisual(m_editIp,ipOK||m_editIp->text().isEmpty());

    m_btnSave->setEnabled(nameOK&&ipOK);
}
