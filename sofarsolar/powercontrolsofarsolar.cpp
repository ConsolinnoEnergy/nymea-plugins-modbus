#include "powercontrolsofarsolar.h"
#include <QObject>
#include <QVector>

PowerControlSofarsolar::PowerControlSofarsolar()
{
    m_nominalPower = 1000;
    m_controlRegister = 0;
    m_limitRegister = 1000; // => 100%
}

unsigned short PowerControlSofarsolar::nominalPower()
{
    return m_nominalPower;
}

void PowerControlSofarsolar::setRelativePowerOutputLimit(unsigned short value)
{
    m_limitRegister = value;
}

void PowerControlSofarsolar::setActivePowerOutputLimit(unsigned short value)
{
    m_limitRegister = value * 1000 / m_nominalPower;
}

void PowerControlSofarsolar::setActivePowerLimitEnable(bool value)
{
    if (value)
        m_controlRegister |= 0x01;
    else
        m_controlRegister &= 0xFFFE;
}

QVector<quint16> PowerControlSofarsolar::Registers()
{
    return QVector<quint16>{m_controlRegister, m_limitRegister};
}

void PowerControlSofarsolar::setNominalPower(unsigned short value)
{
    m_nominalPower = value;
}

bool PowerControlSofarsolar::activePowerLimitEnabled()
{
    return (bool)(m_controlRegister & 0x0001);
}

unsigned short PowerControlSofarsolar::activePowerOutputLimit()
{
    return (unsigned short)(m_nominalPower * m_limitRegister / 1000);
}

double PowerControlSofarsolar::relativePowerLimit()
{
    return m_limitRegister / 10.0;
}
