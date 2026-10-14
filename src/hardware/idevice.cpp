#include "idevice.h"
#include <QDebug>



IDevice::IDevice(const QString &id, QObject *parent)
    :QObject(parent)
    ,m_id(id)
    ,m_state(ConnectState::Disconnected)
{

}

void IDevice::setState(ConnectState state)
{
    if (m_state != state) {
        m_state = state;
        qInfo() << QString("[IDevice:%1] Connection state changed -> %2")
                       .arg(m_id)
                       .arg(static_cast<int>(m_state));
        emit stateChanged(m_state);
    }
}

