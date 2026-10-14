#include "mockdevice.h"

#include <QTimer>
#include <QRandomGenerator>
#include <QtMath>
#include <QDebug>


MockDevice::MockDevice(const QString &id, const MockConfig &config, QObject *parent)
    :IDevice(id,parent)
    ,m_config(config)
{// 初始化默认寄存器基线
    m_registers[REG_TEMPERATURE]   = 400; // 40.0 ℃
    m_registers[REG_PRESSURE]      = 50;  // 5.0 bar
    m_registers[REG_VOLTAGE]       = 3800;// 380.0 V
    m_registers[REG_CONTROL_WORD]  = 1;   // 默认正常运行
    m_registers[REG_CYCLE_COUNTER] = 0;
}

MockDevice::~MockDevice()
{
    MockDevice::close();
}

bool MockDevice::open()
{
    if(m_faultInjected)
    {
        setState(ConnectState::Disconnected);
        return false;
    }
    if(isConnected())
        return true;
    setState(ConnectState::Connecting);

    // 模拟三次握手
    int handShakeDelay=std::max(10,m_config.jitterMs);
    QTimer::singleShot(handShakeDelay,this,[this](){
        if(m_state==ConnectState::Connecting)
        {
            setState(ConnectState::Connected);
            qInfo() << QString("[MockDevice:%1] Handshake complete! Link Connected.").arg(m_id);
        }
    });
    return true;
}

void MockDevice::close()
{
    if(m_state!=ConnectState::Disconnected)
    {
        setState(ConnectState::Disconnected);
        qInfo()<<QString("[MockDevice:%1] channel closed").arg(m_id);
    }
}

bool MockDevice::isConnected() const
{
    return m_state==ConnectState::Connected;
}

bool MockDevice::readRegisters(int startAddress, int count, QList<quint16> &outData)
{
    outData.clear();

    if(m_faultInjected||!isConnected())
    {
        setState(ConnectState::Disconnected);
        return false;
    }

    if (!isConnected()) {
        emit communicationError(QString("[MockDevice:%1] Read failed: Device is not connected!").arg(m_id));
        return false;
    }

    if (startAddress < 0 || startAddress + count > TOTAL_REGISTERS) {
        emit communicationError(QString("[MockDevice:%1] Register range [0x%2 .. 0x%3] out of bounds!")
                                    .arg(m_id).arg(startAddress, 4, 16, QChar('0')).arg(startAddress + count, 4, 16, QChar('0')));
        return false;
    }

    // 动态生成连续物理量
    generatePhysicsTelemetry();

    // 填充 16 位数据缓冲区
    for (int i = 0; i < count; ++i) {
        outData.append(m_registers.value(startAddress + i, 0));
    }

    // 广播数据就绪信号
    emit dataReady(outData);
    return true;
}

bool MockDevice::writeRegister(int address, quint16 value)
{
    if (!isConnected()) {
        emit communicationError(QString("[MockDevice:%1] Write failed: Device is offline!").arg(m_id));
        return false;
    }

    if (address < 0 || address >= TOTAL_REGISTERS) {
        emit communicationError(QString("[MockDevice:%1] Write address 0x%2 invalid!").arg(m_id).arg(address, 4, 16, QChar('0')));
        return false;
    }

    // 更新内部保持寄存器
    m_registers[address] = value;
    qInfo() << QString("[MockDevice:%1] Write SUCCESS -> Reg[0x%2] = %3")
                   .arg(m_id).arg(address, 4, 16, QChar('0')).arg(value);

    return true;
}

void MockDevice::injectFault(bool fault)
{
    m_faultInjected=fault;
    if(m_faultInjected)
        setState(ConnectState::Disconnected);
}

void MockDevice::generatePhysicsTelemetry()
{
    m_timeStep+=0.1;
    // 温度
    double noise=(QRandomGenerator::global()->bounded(100)-50)/100.0*m_config.noiseAmplitude;
    double simTemp=45.0+10.0*qSin(m_timeStep)+noise;
    m_registers[REG_TEMPERATURE]=static_cast<quint16>(qBound(0.0,simTemp,120.0)*10);

    // 压力
    double simPressure=6.0+1.5*qCos(m_timeStep*0.8);
    m_registers[REG_PRESSURE]=static_cast<quint16>(qBound(0.0,simPressure,15.0)*10);

    // 电压
    double simVoltage=380.0+(QRandomGenerator::global()->bounded(40)-20)/10.0;
    m_registers[REG_VOLTAGE]=static_cast<quint16>(simVoltage*10);

    // 周期技术自增
    m_registers[REG_CYCLE_COUNTER]=m_registers[REG_CYCLE_COUNTER]+1;
}

