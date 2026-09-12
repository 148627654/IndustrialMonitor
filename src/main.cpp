#include "mainwindow.h"
#include "views/logindialog.h"
#include <QApplication>
#include "common/thememanager.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 启用热重载与暗黑主题
    ThemeManager::instance().enableHotReload();
    ThemeManager::applyTheme("../res/qss/dark_style.qss");

    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        // 用户取消或直接关闭，阻断主程序启动
        return 0;
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
