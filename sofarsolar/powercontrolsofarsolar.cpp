#include "powercontrolsofarsolar.h"

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

void PowerControlSofarsolar::setAbsolutePowerLimit(unsigned short value)
{
    m_limitRegister = value * 1000 / m_nominalPower;
}

void PowerControlSofarsolar::setPowerLimitEnable(bool value)
{
    if (value)
        m_controlRegister |= 0x01;
    else
        m_controlRegister &= 0xFFFE;
}

void PowerControlSofarsolar::setNominalPower(unsigned short value)
{
    m_nominalPower = value;
}

bool PowerControlSofarsolar::powerLimitEnabled()
{
    return (bool)(m_controlRegister & 0x0001);
}

unsigned short PowerControlSofarsolar::absolutePowerLimit()
{
    return (unsigned short)(m_nominalPower * m_limitRegister / 1000);
}

double PowerControlSofarsolar::relativePowerLimit()
{
    return m_limitRegister / 10.0;
}

unsigned long PowerControlSofarsolar::combinedRegisters()
{
    return (((unsigned long)m_controlRegister) << 16) + m_limitRegister;
}

void PowerControlSofarsolar::setCombinedRegisters(unsigned long value)
{
    m_limitRegister = (unsigned short)value;
    m_controlRegister = (unsigned short)(value >> 16);
}
