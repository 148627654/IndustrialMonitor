#ifndef DEVICEFACTORY_H
#define DEVICEFACTORY_H

#include "../common/DeviceDef.h"
#include "idevice.h"
#include <memory>

/**
 * @brief 工业通信设备多态简单工厂 (Simple Factory)
 * @details 负责根据 DeviceInfo 中的通信协议类型与网络配置，反射创建具体的硬件驱动实例。
 *          上层业务只面对 IDevice 纯虚接口，彻底实现协议解耦与所有权清晰移交。
 * @author Yuquan Guo
 * @date 2026-09
 */
class DeviceFactory
{
public:
    static std::unique_ptr<IDevice> createDevice(const DeviceInfo &info);
private:
    DeviceFactory()=delete;
};

#endif // DEVICEFACTORY_H
