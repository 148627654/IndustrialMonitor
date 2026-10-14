#include "devicedao.h"
#include "../db/databasemanager.h"
#include <QSqlError>
DeviceInfo DeviceDao::recordToDeviceInfo(const QSqlQuery &query)
{
    DeviceInfo dev;
    dev.id          = query.value("f_id").toString();
    dev.name        = query.value("f_name").toString();
    dev.ip          = query.value("f_ip").toString();
    dev.port        = query.value("f_port").toInt();
    dev.status      = static_cast<DeviceStatus>(query.value("f_status").toInt());
    dev.temperature = query.value("f_temperature").toDouble();
    dev.cpuUsage    = query.value("f_cpu_usage").toDouble();
    dev.memoryUsage = query.value("f_mem_usage").toDouble();
    dev.protocol = static_cast<ProtocolType>(query.value("f_protocol").toInt());
    dev.config   = DeviceConfig::fromJsonString(dev.protocol, query.value("f_config_json").toString());
    return dev;
}

bool DeviceDao::insertDevice(const DeviceInfo &device)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    query.prepare(R"(
        INSERT INTO t_device (
            f_id, f_name, f_ip, f_port, f_status,
            f_temperature, f_cpu_usage, f_mem_usage,
            f_created_at, f_updated_at, f_protocol, f_config_json
        ) VALUES (
            :id, :name, :ip, :port, :status,
            :temp, :cpu, :mem,
            CURRENT_TIMESTAMP, CURRENT_TIMESTAMP,
            :protocol, :config_json   -- ✅ 必须加上这两个占位符！
        );
    )");

    query.bindValue(":id",          device.id);
    query.bindValue(":name",        device.name);
    query.bindValue(":ip",          device.ip);
    query.bindValue(":port",        device.port);
    query.bindValue(":status",      static_cast<int>(device.status));
    query.bindValue(":temp",        device.temperature);
    query.bindValue(":cpu",         device.cpuUsage);
    query.bindValue(":mem",         device.memoryUsage);
    query.bindValue(":protocol",    static_cast<int>(device.protocol));
    query.bindValue(":config_json", device.config.toJsonString());

    if (!query.exec())
    {
        qWarning() << "[DAO] Failed to insert device:" << device.id
                   << "| Error:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DeviceDao::updateDevice(const DeviceInfo &device)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    query.prepare(R"(
        UPDATE t_device SET
            f_name = :name,
            f_ip = :ip,
            f_port = :port,
            f_status = :status,
            f_temperature = :temp,
            f_cpu_usage = :cpu,
            f_mem_usage = :mem,
            f_updated_at = CURRENT_TIMESTAMP,
            f_protocol = :protocol,
            f_config_json = :config_json
        WHERE f_id = :id;
    )");

    query.bindValue(":id",     device.id);
    query.bindValue(":name",   device.name);
    query.bindValue(":ip",     device.ip);
    query.bindValue(":port",   device.port);
    query.bindValue(":status", static_cast<int>(device.status));
    query.bindValue(":temp",   device.temperature);
    query.bindValue(":cpu",    device.cpuUsage);
    query.bindValue(":mem",    device.memoryUsage);
    query.bindValue(":protocol",    static_cast<int>(device.protocol));
    query.bindValue(":config_json", device.config.toJsonString());

    if (!query.exec()) {
        qWarning() << "[DAO] Failed to update device:" << device.id
                   << "| Error:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool DeviceDao::deleteDeviceById(const QString &id)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    query.prepare("DELETE FROM t_device WHERE f_id = :id;");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "[DAO] Failed to delete device:" << id
                   << "| Error:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

std::optional<DeviceInfo> DeviceDao::queryDeviceById(const QString &id)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    query.prepare("SELECT * FROM t_device WHERE f_id = :id LIMIT 1;");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        return recordToDeviceInfo(query);
    }
    return std::nullopt;
}

QList<DeviceInfo> DeviceDao::queryAllDevices()
{
    QList<DeviceInfo> list;
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    // 默认按设备编号升序检索全量数据
    if (!query.exec("SELECT * FROM t_device ORDER BY f_id ASC;")) {
        qWarning() << "[DAO] Failed to query all devices | Error:" << query.lastError().text();
        return list;
    }

    while (query.next()) {
        list.append(recordToDeviceInfo(query));
    }
    return list;
}

bool DeviceDao::existsById(const QString &id)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    query.prepare("SELECT 1 FROM t_device WHERE f_id = :id LIMIT 1;");
    query.bindValue(":id", id);

    return query.exec() && query.next();
}

bool DeviceDao::batchInsertDevices(const QList<DeviceInfo> &devices, bool replaceOnConflict)
{
    if (devices.isEmpty()) {
        return true;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.isOpen()) {
        qCritical() << "[DAO] Database is not open during batchInsertDevices.";
        return false;
    }

    // 1. 显式开启事务
    if (!db.transaction()) {
        qCritical() << "[DAO] Failed to start transaction:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    QString sql = replaceOnConflict ?
                      R"(INSERT OR REPLACE INTO t_device (
            f_id, f_name, f_ip, f_port, f_status,
            f_temperature, f_cpu_usage, f_mem_usage,
            f_created_at, f_updated_at
        ) VALUES (
            :id, :name, :ip, :port, :status,
            :temp, :cpu, :mem,
            CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
        );)" :
                      R"(INSERT INTO t_device (
            f_id, f_name, f_ip, f_port, f_status,
            f_temperature, f_cpu_usage, f_mem_usage,
            f_created_at, f_updated_at
        ) VALUES (
            :id, :name, :ip, :port, :status,
            :temp, :cpu, :mem,
            CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
        );)";

    if (!query.prepare(sql)) {
        qCritical() << "[DAO] Failed to prepare batch insert SQL:" << query.lastError().text();
        db.rollback();
        return false;
    }

    // 2. 在事务保护下循环快速插入
    for (const DeviceInfo &dev : devices) {
        query.bindValue(":id",     dev.id);
        query.bindValue(":name",   dev.name);
        query.bindValue(":ip",     dev.ip);
        query.bindValue(":port",   dev.port);
        query.bindValue(":status", static_cast<int>(dev.status));
        query.bindValue(":temp",   dev.temperature);
        query.bindValue(":cpu",    dev.cpuUsage);
        query.bindValue(":mem",    dev.memoryUsage);

        if (!query.exec()) {
            qCritical() << "[DAO] Batch insert failed on device:" << dev.id
                        << "| Error:" << query.lastError().text();
            // 核心机制：遇错立即全量回滚，绝不留半截脏数据！
            db.rollback();
            return false;
        }
    }

    // 3. 全量无误，单次合并刷盘提交
    if (!db.commit()) {
        qCritical() << "[DAO] Failed to commit batch transaction:" << db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

bool DeviceDao::batchDeleteDevices(const QStringList &ids)
{
    if (ids.isEmpty()) {
        return true;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        qCritical() << "[DAO] Failed to start transaction for batch delete:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM t_device WHERE f_id = :id;");

    for (const QString &id : ids) {
        query.bindValue(":id", id);
        if (!query.exec()) {
            qCritical() << "[DAO] Batch delete failed on ID:" << id << query.lastError().text();
            db.rollback();
            return false;
        }
    }

    return db.commit();
}