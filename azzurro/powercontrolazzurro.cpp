#include "powercontrolazzurro.h"
#include <QObject>
#include <QVector>

PowerControlAzzurro::PowerControlAzzurro()
{
    m_nominalPower = 1000;
    m_controlRegister = 0;
    m_limitRegister = 1000; // => 100%
}

unsigned short PowerControlAzzurro::nominalPower()
{
    return m_nominalPower;
}

void PowerControlAzzurro::setRelativePowerOutputLimit(unsigned short value)
{
    m_limitRegister = value;
}

void PowerControlAzzurro::setActivePowerOutputLimit(unsigned short value)
{
    m_limitRegister = value * 1000 / m_nominalPower;
}

void PowerControlAzzurro::setActivePowerLimitEnable(bool value)
{
    if (value)
        m_controlRegister |= 0x01;
    else
        m_controlRegister &= 0xFFFE;
}

QVector<quint16> PowerControlAzzurro::Registers()
{
    return QVector<quint16>{m_controlRegister, m_limitRegister};
}

void PowerControlAzzurro::setNominalPower(unsigned short value)
{
    m_nominalPower = value;
}

bool PowerControlAzzurro::activePowerLimitEnabled()
{
    return (bool)(m_controlRegister & 0x0001);
}

unsigned short PowerControlAzzurro::activePowerOutputLimit()
{
    return (unsigned short)(m_nominalPower * m_limitRegister / 1000);
}

double PowerControlAzzurro::relativePowerLimit()
{
    return m_limitRegister / 10.0;
}
