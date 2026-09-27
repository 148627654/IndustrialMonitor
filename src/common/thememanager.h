#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>
#include <QObject>
#include <QEvent>

/**
 * @brief 全局主题管理器 (QSS 主题加载与 F5 热重载单例工具)
 * @details Meyers 单例实现，持有当前 QSS 路径并负责向全局 qApp 应用样式表；
 *          通过 qApp 事件过滤器捕获 F5 键触发热重载，实现"改样式即刷新"的
 *          免重启调试工作流；拷贝构造与赋值运算符均已删除，杜绝多实例分裂。
 * @author Yuquan Guo
 * @date 2026-09
 */
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
