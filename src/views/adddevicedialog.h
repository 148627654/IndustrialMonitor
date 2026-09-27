#ifndef ADDDEVICEDIALOG_H
#define ADDDEVICEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include "../common/DeviceDef.h"

class AddDeviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDeviceDialog(QWidget *parent = nullptr);
    DeviceInfo getDeviceInfo() const;

private:
    QLineEdit *m_editId;
    QLineEdit *m_editName;
    QLineEdit *m_editIp;
    QSpinBox  *m_spinPort;
    QComboBox *m_comboStatus;
};
#endif // ADDDEVICEDIALOG_H
