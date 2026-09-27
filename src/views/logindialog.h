#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QAction>
namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
private slots:
    void onTogglePasswordEcho();
    void onLoginClicked();
private:
    void initUI();
    void loadConfig();
    void saveConfig();

    QAction *m_toggleAction;                        //密码的可视化,自定义action
    bool m_isPasswordHidden;
private:
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
