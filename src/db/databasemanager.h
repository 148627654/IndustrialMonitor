#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>

/**
 * @brief 工业级 SQLite 数据库全局连接与生命周期管理器 (单例模式)
 * @details 采用具名连接隔离机制，物理路径锚定于 bin/industrial_monitor.db，
 *          开启 WAL 预写日志模式，严格防御 QSqlDatabase 成员变量连接泄漏。
 * @author Yuquan Guo
 * @date 2026-09
 */

class DatabaseManager
{
public:
    static DatabaseManager& instance();

    // 初始化并打开数据库连结(执行PRAGMA)
    bool openDatabase();

    // 安全关闭并从全局连接池中住校连结
    void closeDatabase();

    // 按需提取底层有效连结句柄
    QSqlDatabase database() const;

    // 检查当前连结是否处于打开状态
    bool isConnected() const;

    //获取当前连结名称
    QString connectionName() const {return m_connectionName;}
private:
    DatabaseManager();
    ~DatabaseManager();

    //禁用拷贝 赋值
    DatabaseManager(const DatabaseManager&)=delete;
    DatabaseManager& operator=(const DatabaseManager&)=delete;

    //执行工业性能与安全性PRAGMA
    bool executePragmas(QSqlDatabase &db);

    const QString m_connectionName;
};

#endif // DATABASEMANAGER_H
