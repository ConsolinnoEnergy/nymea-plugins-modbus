#include "alphatecWallbox.h"

AlphatecWallbox::AlphatecWallbox(AlphatecWallboxModbusTcpConnection *modbusTcpConnection, QObject *parent) :
    QObject{parent},
    m_modbusTcpConnection{modbusTcpConnection}
{
}
