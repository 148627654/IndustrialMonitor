#ifndef ADDDEVICEDIALOG_H
#define ADDDEVICEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include "common/DeviceDef.h"

class AddDeviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDeviceDialog(QWidget *parent = nullptr);
    DeviceInfo getDeviceInfo() const;

private slots:
    void validateForm(); // 实时校验表单各字段完整性

private:
    void updateInputVisual(QLineEdit *edit, bool valid);

    QLineEdit   *m_editId;
    QLineEdit   *m_editName;
    QLineEdit   *m_editIp;
    QSpinBox    *m_spinPort;
    QComboBox   *m_comboStatus;
    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;
};
#endif // ADDDEVICEDIALOG_H
