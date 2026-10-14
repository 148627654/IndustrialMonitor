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

    if(!initTables())
    {
        qCritical() << "[DB] Failed to initialize database tables.";
        return false;
    }

    if(!initSeedData())
        qWarning() << "[DB] Seed data initialization encountered an issue.";

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

bool DatabaseManager::initTables()
{
    QSqlDatabase db=database();
    QSqlQuery query(db);

    // 1. 创建生产级设备台账表 (t_device)
    const QString createDeviceTableSql = R"(
        CREATE TABLE IF NOT EXISTS t_device (
            f_id          TEXT PRIMARY KEY,
            f_name        TEXT NOT NULL,
            f_ip          TEXT NOT NULL,
            f_port        INTEGER NOT NULL DEFAULT 502,
            f_status      INTEGER NOT NULL DEFAULT 0,
            f_temperature REAL NOT NULL DEFAULT 0.0,
            f_cpu_usage   REAL NOT NULL DEFAULT 0.0,
            f_mem_usage   REAL NOT NULL DEFAULT 0.0,
            f_created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
            f_updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    if(!query.exec(createDeviceTableSql))
    {
        qCritical() << "[DB] Failed to create table 't_device':" << query.lastError().text();
        return false;
    }
    qInfo() << "[DB] Table 't_device' checked/created successfully.";

    // IP 字段建立B树 索引
    const QString createIndexSql = "CREATE INDEX IF NOT EXISTS idx_device_ip ON t_device(f_ip);";
    if (!query.exec(createIndexSql)) {
        qWarning() << "[DB] Failed to create index 'idx_device_ip':" << query.lastError().text();
    } else {
        qInfo() << "[DB] Index 'idx_device_ip' checked/created successfully.";
    }

    // 创建系统参数配置表
    const QString createConfigTableSql = R"(
        CREATE TABLE IF NOT EXISTS t_sys_config (
            f_key   TEXT PRIMARY KEY,
            f_value TEXT NOT NULL,
            f_desc  TEXT
        );
    )";
    if (!query.exec(createConfigTableSql)) {
        qCritical() << "[DB] Failed to create table 't_sys_config':" << query.lastError().text();
        return false;
    }
    qInfo() << "[DB] Table 't_sys_config' checked/created successfully.";
    auto checkAndAddColumn = [&](const QString &colName, const QString &colDef) {
        QSqlQuery checkQuery(db);
        checkQuery.exec("PRAGMA table_info(t_device);");
        bool exists = false;
        while (checkQuery.next()) {
            if (checkQuery.value("name").toString().compare(colName, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            qInfo() << QString("[DB Migration] Upgrading schema: Adding column '%1' to 't_device'...").arg(colName);
            QSqlQuery alterQuery(db);
            QString alterSql = QString("ALTER TABLE t_device ADD COLUMN %1 %2;").arg(colName, colDef);
            if (!alterQuery.exec(alterSql)) {
                qCritical() << "[DB Migration] Failed to alter table:" << alterQuery.lastError().text();
                return false;
            }
        }
        return true;
    };

    // 动态打入新版本列补丁
    checkAndAddColumn("f_protocol", "INTEGER NOT NULL DEFAULT 1");
    checkAndAddColumn("f_config_json", "TEXT DEFAULT '{}'");

    return true;
}

bool DatabaseManager::isTableEmpty(const QString &tableName)
{
    QSqlDatabase db = database();
    QSqlQuery query(db);
    QString sql = QString("SELECT COUNT(*) FROM %1;").arg(tableName);
    if (query.exec(sql) && query.next()) {
        return query.value(0).toInt() == 0;
    }
    return true;
}

bool DatabaseManager::initSeedData()
{
    // 检查设备表是否为空，非空则绝不覆盖用户数据 (幂等性保证)
    if (!isTableEmpty("t_device")) {
        qInfo() << "[DB] Table 't_device' already has records. Skipping seed data.";
        return true;
    }

    QSqlDatabase db = database();
    db.transaction(); // 开启事务原子写入

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO t_device (f_id, f_name, f_ip, f_port, f_status, f_temperature, f_cpu_usage, f_mem_usage)
        VALUES (:id, :name, :ip, :port, :status, :temp, :cpu, :mem);
    )");

    struct SeedItem {
        const char* id;
        const char* name;
        const char* ip;
        int port;
        int status;
        double temp;
        double cpu;
        double mem;
    };

    SeedItem seeds[] = {
        {"DEV-1001", "1号高速注塑机", "192.168.1.101", 502, 1, 42.5, 28.0, 35.0},
        {"DEV-1002", "2号高速注塑机", "192.168.1.102", 502, 1, 45.0, 32.5, 38.0},
        {"DEV-1003", "3号重型冲压机", "192.168.1.103", 502, 2, 78.5, 68.0, 52.0},
        {"DEV-1004", "4号精密压铸机", "192.168.1.104", 502, 1, 55.2, 44.0, 48.0},
        {"DEV-1005", "5号装配机械臂", "192.168.1.105", 502, 3, 88.0, 92.0, 78.0},
        {"DEV-1006", "6号数控加工中心", "192.168.1.106", 502, 1, 38.0, 25.0, 30.0},
        {"DEV-1007", "7号空压动力机组", "192.168.1.107", 502, 0,  0.0,  0.0,  0.0},
        {"DEV-1008", "8号热处理淬火炉", "192.168.1.108", 502, 1, 65.0, 50.0, 55.0},
        {"DEV-1009", "9号AGV自动调度站", "192.168.1.109", 502, 1, 36.5, 18.0, 24.0},
        {"DEV-1010", "10号成品码垛机", "192.168.1.110", 502, 0,  0.0,  0.0,  0.0}
    };

    for (const auto &item : seeds) {
        query.bindValue(":id", item.id);
        query.bindValue(":name", item.name);
        query.bindValue(":ip", item.ip);
        query.bindValue(":port", item.port);
        query.bindValue(":status", item.status);
        query.bindValue(":temp", item.temp);
        query.bindValue(":cpu", item.cpu);
        query.bindValue(":mem", item.mem);

        if (!query.exec()) {
            qCritical() << "[DB] Failed to insert seed device:" << item.id << query.lastError().text();
            db.rollback(); // 遇错立即回滚
            return false;
        }
    }

    // 初始化系统配置表种子参数
    if (isTableEmpty("t_sys_config")) {
        QSqlQuery configQuery(db);
        configQuery.prepare("INSERT INTO t_sys_config (f_key, f_value, f_desc) VALUES (:k, :v, :d);");

        auto insertConfig = [&](const QString &k, const QString &v, const QString &d) {
            configQuery.bindValue(":k", k);
            configQuery.bindValue(":v", v);
            configQuery.bindValue(":d", d);
            configQuery.exec();
        };

        insertConfig("Telemetry_Interval_Ms", "1000", "传感器遥测高频采集周期(ms)");
        insertConfig("Warning_Temp_Threshold", "75.0", "设备温度黄色预警阈值(℃)");
        insertConfig("Danger_Temp_Threshold", "85.0", "设备温度红色报警阈值(℃)");
    }

    db.commit(); // 提交事务
    qInfo() << "[DB] Database is clean. Successfully seeded 10 initial industrial devices & configs.";
    return true;
}