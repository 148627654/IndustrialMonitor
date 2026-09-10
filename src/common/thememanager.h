#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>
#include <QObject>
#include <QEvent>
class ThemeManager: public QObject
{
    Q_OBJECT;
public:
    static ThemeManager& instance();

    // 加载指定路径的 QSS
    static void applyTheme(const QString &qssPath);

    // 重新加载当前 QSS
    static void reloadTheme();

    // 启用 F5 热重载调试支持
    void enableHotReload();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    ThemeManager() = default;
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    static QString s_currentQssPath;
};

#endif // THEMEMANAGER_H
