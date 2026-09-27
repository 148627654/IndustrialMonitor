#include "mainwindow.h"
#include "views/logindialog.h"
#include <QApplication>
#include "common/thememanager.h"
#include "db/databasemanager.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 全局样式与热重载挂载
    ThemeManager::instance().enableHotReload();
    ThemeManager::applyTheme(":/qss/dark_style.qss");

    // 2. 数据库底层基座初始化 (开工第一步)
    if (!DatabaseManager::instance().openDatabase()) {
        qCritical() << "[Main] Fatal: Could not establish database connection. Aborting.";
        return -1;
    }

    // 3. 身份认证模态拦截
    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        // 用户取消或直接关闭，阻断主程序启动前安全释放数据库
        DatabaseManager::instance().closeDatabase();
        return 0;
    }

    // 4. 校验通过，唤醒监控系统主窗口
    MainWindow w;
    w.show();

    int exitCode = a.exec();

    // 5. 程序退出前，显式安全关闭数据库
    DatabaseManager::instance().closeDatabase();

    return exitCode;
}