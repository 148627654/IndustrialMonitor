#ifndef DEVICEDAO_H
#define DEVICEDAO_H

#include "../common/DeviceDef.h"
#include <QSqlQuery>
#include <optional>
#include <QString>
/**
 * @brief 设备台账数据访问对象 (Data Access Object)
 * @details 负责设备实体 DeviceInfo 与 SQLite 数据库表 t_device 之间的双向无损映射，
 *          全量使用预编译参数化查询防御 SQL 注入。
 * @author Yuquan Guo
 * @date 2026-09
 */

class DeviceDao
{
    DeviceDao()=delete;
    // 辅助工具：将当前游标行映射反序列化为 DeviceInfo 结构体
    static DeviceInfo recordToDeviceInfo(const class QSqlQuery &query);
public:

    // 增删改查
    static bool insertDevice(const DeviceInfo &device);
    static bool deleteDeviceById(const QString &id);        // 通过主键删除
    static bool updateDevice(const DeviceInfo &device);     // 改
    // 根据主键精确单查
    static std::optional<DeviceInfo> queryDeviceById(const QString &id);

    // 查询全量设备列表
    static QList<DeviceInfo> queryAllDevices();

    // 检查设备主键是否已存在
    static bool existsById(const QString &id);

    /**
     * @brief 批量插入设备记录 (原子事务保护)
     * @param devices 待插入的设备实体列表
     * @param replaceOnConflict 主键冲突时是否替换原记录 (默认 false，遇冲突立即回滚)
     * @return 全部插入成功返回 true；任意单条失败触发全量回滚并返回 false
     */
    static bool batchInsertDevices(const QList<DeviceInfo> &devices, bool replaceOnConflict = false);

    /**
     * @brief 批量删除指定 ID 的设备记录 (原子事务保护)
     * @param ids 待删除的设备编号列表
     * @return 全部删除成功返回 true，发生异常回滚返回 false
     */
    static bool batchDeleteDevices(const QStringList &ids);
};

#endif // DEVICEDAO_H
