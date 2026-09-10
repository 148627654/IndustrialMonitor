#include "mainwindow.h"

#include <QApplication>
#include "common/thememanager.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 启用热重载与暗黑主题
    ThemeManager::instance().enableHotReload();
    ThemeManager::applyTheme("../res/qss/dark_style.qss");
    MainWindow w;
    w.show();
    return QApplication::exec();
}
