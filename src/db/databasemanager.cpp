#include "databasemanager.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::openDatabase()
{
    //已经打开
    if(QSqlDatabase::contains(m_connectionName))
    {
        QSqlDatabase existingDB=QSqlDatabase::database(m_connectionName);
        if(existingDB.isOpen())
            return true;
    }

    // 物理路径
    QString dbDir=QCoreApplication::applicationDirPath();
    QString dbPath=dbDir+"/industrial_monitor.db";

    qInfo()<<"[DB] Initializing SQLite database at:" << dbPath;

    // 建立连接
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",m_connectionName);
    db.setDatabaseName(dbPath);

    // 打开数据库
    if (!db.open())
    {
        qCritical() << "[DB] Failed to open database:" << db.lastError().text();
        return false;
    }

    // 执行工业级 PRAGMA 调优
    if (!executePragmas(db)) {
        qWarning() << "[DB] Warning: PRAGMA optimizations could not be fully applied.";
    }

    qInfo() << "[DB] SQLite database connected successfully with WAL mode enabled.";
    return true;
}

DatabaseManager::DatabaseManager()
    : m_connectionName("IndustrialMonitor_Main_Connection")
{
}

DatabaseManager::~DatabaseManager()
{
    // 析构兜底
    closeDatabase();
}

bool DatabaseManager::executePragmas(QSqlDatabase &db)
{
    QSqlQuery query(db);

    // ① 强制开启外键约束 (SQLite 默认关闭外键级联)
    if (!query.exec("PRAGMA foreign_keys = ON;")) {
        qWarning() << "[DB] Failed to enable foreign_keys:" << query.lastError().text();
        return false;
    }

    // ② 开启 WAL (Write-Ahead Logging) 预写日志，并发读写性能大幅提升
    if (!query.exec("PRAGMA journal_mode = WAL;")) {
        qWarning() << "[DB] Failed to set journal_mode to WAL:" << query.lastError().text();
        return false;
    } else {
        if (query.next()) {
            qInfo() << "[DB] Current journal mode confirmed as:" << query.value(0).toString();
        }
    }

    // ③ 设置同步模式为 NORMAL (在 WAL 模式下兼顾高吞吐与断电安全性)
    if (!query.exec("PRAGMA synchronous = NORMAL;")) {
        qWarning() << "[DB] Failed to set synchronous to NORMAL:" << query.lastError().text();
        return false;
    }

    return true;
}

void DatabaseManager::closeDatabase()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            // 局部作用域显式关闭连接，确保局部 QSqlDatabase 变量被销毁
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) {
                db.close();
                qInfo() << "[DB] Database connection closed.";
            }
        }
        // 彻底安全地从全局连接池中移除
        QSqlDatabase::removeDatabase(m_connectionName);
        qInfo() << "[DB] Database connection removed from pool cleanly.";
    }
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool DatabaseManager::isConnected() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }
    return QSqlDatabase::database(m_connectionName).isOpen();
}

