#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include <QString>
#include <QJsonObject>
#include <variant>

// 通信协议枚举
enum class ProtocolType{
    Mock=0,             // 虚拟仿真驱动(内存生成)
    ModbusTcp=1,        // 以太网
    ModbusRtu=2         // 串行总线(RS485)
};

// 以太网TCP专属配置
struct TcpConfig{
    int timeoutMs=3000;
    bool keepAlive=true;        // 保持长连接心跳
};

// 串口配置
struct SerialConfig{
    QString portName="COM1";        // 串口名字
    int baudRate=9600;              // 波特率
    int dataBits=8;                 // 数据位
    int parity=0;                   // 校验位
    int stopBits=1;                 // 停止位
};

// 虚拟仿真配置
struct MockConfig{
    int jitterMs=20;                // 模拟网络抖动延迟
    double noiseAmplitude=0.5;      // 物理噪声振幅
};

/**
 * @brief 异构多协议通信参数封装实体
 * @details 采用 C++17 std::variant 类型安全联合体，集成 QJsonObject 序列化通道。
 * @author Yuquan Guo
 * @date 2026-09
 */

class DeviceConfig
{
public:
    using ConfigPayload = std::variant<TcpConfig, SerialConfig, MockConfig>;

    DeviceConfig();
    explicit DeviceConfig(const TcpConfig &tcp);
    explicit DeviceConfig(const SerialConfig &serial);
    explicit DeviceConfig(const MockConfig &mock);

    // 载荷提取接口
    ConfigPayload payload() const { return m_payload; }
    void setPayload(const ConfigPayload &payload) { m_payload = payload; }

    // 快速类型检测
    bool isTcp() const { return std::holds_alternative<TcpConfig>(m_payload); }
    bool isSerial() const { return std::holds_alternative<SerialConfig>(m_payload); }
    bool isMock() const {return std::holds_alternative<MockConfig>(m_payload);}

    TcpConfig asTcp() const { return std::get<TcpConfig>(m_payload); }
    SerialConfig asSerial() const { return std::get<SerialConfig>(m_payload); }
    MockConfig asMock() const { return std::get<MockConfig>(m_payload); }
    // JSON 双向序列化协议
    QString toJsonString() const;
    static DeviceConfig fromJsonString(ProtocolType type, const QString &jsonStr);

private:
    ConfigPayload m_payload;
};

#endif // DEVICECONFIG_H
