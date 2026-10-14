#include "devicefactory.h"
#include <QDebug>
#include "mock/mockdevice.h"

std::unique_ptr<IDevice> DeviceFactory::createDevice(const DeviceInfo &info)
{
    switch (info.protocol) {
    case ProtocolType::Mock:        // 仿真
    {
        MockConfig cfg;
        if(info.config.isMock())
            cfg=info.config.asMock();
        else
        {
            cfg.jitterMs=25;
            cfg.noiseAmplitude=0.5;
        }
        qInfo() << QString("[DeviceFactory] Creating MockDevice instance for: %1 (%2)").arg(info.name, info.id);
        return std::make_unique<MockDevice>(info.id, cfg);
    }
    case ProtocolType::ModbusTcp: {
        // [V3 规划扩展点] 待接入正式以太网 ModbusTcpDevice
        qWarning() << QString("[DeviceFactory] ModbusTcpDriver for %1 not yet registered in V2. Falling back to MockDriver.")
                          .arg(info.id);
        return std::make_unique<MockDevice>(info.id, MockConfig{});
    }

    case ProtocolType::ModbusRtu: {
        // [V3 规划扩展点] 待接入串口 ModbusRtuDevice
        qWarning() << QString("[DeviceFactory] ModbusRtuDriver for %1 not yet registered in V2. Falling back to MockDriver.")
                          .arg(info.id);
        return std::make_unique<MockDevice>(info.id, MockConfig{});
    }

    default:
        qCritical() << QString("[DeviceFactory] Unknown protocol type for device: %1").arg(info.id);
        return nullptr;
    }
}
