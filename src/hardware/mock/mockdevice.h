#ifndef MOCKDEVICE_H
#define MOCKDEVICE_H

#include <QObject>
#include <idevice.h>
#include <QString>
#include "../deviceconfig.h"
#include <QMap>
/**
 * @brief 工业级虚拟仿真设备驱动 (Mock Hardware Driver)
 * @details 继承自 IDevice，模拟网络三次握手时延、正弦波连续传感器物理信号与 16 位工控寄存器读写。
 * @author Yuquan Guo
 * @date 2026-09
 */
class MockDevice : public IDevice
{
    Q_OBJECT
public:
    explicit MockDevice(const QString &id, const MockConfig &config = MockConfig{}, QObject *parent = nullptr);
    ~MockDevice() override;

    // 实现 IDevice 核心纯虚接口
    bool open() override;
    void close() override;
    bool isConnected() const override;
    bool readRegisters(int startAddress, int count, QList<quint16> &outData) override;
    bool writeRegister(int address, quint16 value) override;

    // 寄存器地址常量定义
    static constexpr int REG_TEMPERATURE   = 0x0000; // 温度 (Scale: 0.1 ℃)
    static constexpr int REG_PRESSURE      = 0x0001; // 压力 (Scale: 0.1 bar)
    static constexpr int REG_VOLTAGE       = 0x0002; // 电压 (Scale: 0.1 V)
    static constexpr int REG_CONTROL_WORD  = 0x0003; // 控制字 (0:停机, 1:运行)
    static constexpr int REG_CYCLE_COUNTER = 0x0004; // 运转周期计数器
    static constexpr int TOTAL_REGISTERS   = 0x0005;

    // ==========================================
    // 核心新增：测试与状态机故障注入接口
    // ==========================================
    void injectFault(bool fault) override;
    bool isFaulty() const { return m_faultInjected; }

private:
    void generatePhysicsTelemetry(); // 计算并刷新当前时刻的连续物理量

    MockConfig m_config;
    double m_timeStep{ 0.0 };         // 模拟时间积分步长 (用于正弦曲线驱动)
    QMap<int, quint16> m_registers;  // 内部保持寄存器映射缓存

    // 强制切断通道
    bool m_faultInjected{false};
};

#endif // MOCKDEVICE_H
