#ifndef EDITDEVICEDIALOG_H
#define EDITDEVICEDIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include "../common/DeviceDef.h"
class EditDeviceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditDeviceDialog(const DeviceInfo &dev, QWidget *parent = nullptr);
    DeviceInfo getDeviceInfo() const;
private slots:
    void validateForm();
private:
    void updateInputVisual(QLineEdit *edit,bool valid);
    DeviceInfo m_originalDev;       // 保存原始对象

    QLineEdit       *m_editId;
    QLineEdit       *m_editName;
    QLineEdit       *m_editIp;
    QSpinBox        *m_spinPort;
    QComboBox       *m_comboStatus;
    QPushButton     *m_btnSave;
    QPushButton     *m_btnCancel;
};

#endif // EDITDEVICEDIALOG_H
