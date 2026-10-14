#ifndef IDEVICE_H
#define IDEVICE_H

#include <QObject>
#include <QString>
#include <QList>

/**
 * @brief 工业通信硬件设备抽象基类 (Hardware Abstraction Layer)
 * @details 遵循依赖倒置原则(DIP)，定义所有物理/虚拟设备的通用通信原语、寄存器读写契约与异步信号。
 *          上层监控逻辑未来只面对 IDevice 指针，彻底解耦具体协议细节。
 * @author Yuquan Guo
 * @date 2026-09
 */
class IDevice : public QObject
{
    Q_OBJECT

public:
    // 工业设备通信五态状态机
    enum class ConnectState {
        Disconnected,  // 已断开
        Connecting,    // 正在建立握手
        Connected,     // 正常在线
        Reconnecting,  // 异常重连中
        Error          // 通信故障
    };
    Q_ENUM(ConnectState)

    explicit IDevice(const QString &id, QObject *parent = nullptr);
    virtual ~IDevice() = default; // 虚析构：保证派生类多态安全释放

    // ==================== [核心纯虚通信原语] ====================
    virtual bool open() = 0;                                                   // 打开物理/虚拟通道
    virtual void close() = 0;                                                  // 断开通道
    virtual bool isConnected() const = 0;                                      // 是否处于可用连通态
    virtual bool readRegisters(int startAddress, int count, QList<quint16> &outData) = 0; // 采集 16 位保持寄存器
    virtual bool writeRegister(int address, quint16 value) = 0;                // 下发控制单个寄存器

    // ==================== [通用资产属性访问] ====================
    QString deviceId() const { return m_id; }
    ConnectState state() const { return m_state; }

    // 测试/诊断专用故障注入钩子 (默认空实现，真实硬件驱动按需覆写)
    virtual void injectFault(bool fault) { Q_UNUSED(fault) }

    // 供派生类与调度层流转驱动状态并统一广播
    // (严禁声明在 signals 段：否则 moc 自动生成定义，与 idevice.cpp 的真实实现链接冲突)
    void setState(ConnectState state);

signals:
    // 通信链路状态转移信号
    void stateChanged(IDevice::ConnectState newState);

    // 传感器遥测数据采集就绪信号
    void dataReady(const QList<quint16> &data);

    // 通信总线异常告警信号
    void communicationError(const QString &errMsg);

protected:
    QString m_id;                                       // 绑定的资产编号 (如 "DEV-1001")
    ConnectState m_state{ ConnectState::Disconnected }; // 当前通信状态
};


#endif // IDEVICE_H
