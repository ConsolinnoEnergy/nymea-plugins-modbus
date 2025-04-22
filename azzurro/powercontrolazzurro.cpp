#include "powercontrolazzurro.h"

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

void PowerControlAzzurro::setAbsolutePowerLimit(unsigned short value)
{
    m_limitRegister = value * 1000 / m_nominalPower;
}

void PowerControlAzzurro::setPowerLimitEnable(bool value)
{
    if (value)
        m_controlRegister |= 0x01;
    else
        m_controlRegister &= 0xFFFE;
}

void PowerControlAzzurro::setNominalPower(unsigned short value)
{
    m_nominalPower = value;
}

bool PowerControlAzzurro::powerLimitEnabled()
{
    return (bool)(m_controlRegister & 0x0001);
}

unsigned short PowerControlAzzurro::absolutePowerLimit()
{
    return (unsigned short)(m_nominalPower * m_limitRegister / 1000);
}

double PowerControlAzzurro::relativePowerLimit()
{
    return m_limitRegister / 10.0;
}

unsigned long PowerControlAzzurro::combinedRegisters()
{
    return (((unsigned long)m_controlRegister) << 16) + m_limitRegister;
}

void PowerControlAzzurro::setCombinedRegisters(unsigned long value)
{
    m_limitRegister = (unsigned short)value;
    m_controlRegister = (unsigned short)(value >> 16);
}
