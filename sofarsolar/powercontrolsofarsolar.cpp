#include "powercontrolsofarsolar.h"
#include <QObject>
#include <QVector>

PowerControlSofarsolar::PowerControlSofarsolar()
{
    m_nominalPower = 1000;
    m_controlRegister = 0;
    m_limitRegister = 100;
}

unsigned short PowerControlSofarsolar::nominalPower()
{
    return m_nominalPower;
}

void PowerControlSofarsolar::setExportLimitRate(float value)
{
    m_limitRegister = value;

    m_exportLimit = m_nominalPower * m_limitRegister / 100;
    if (m_exportLimit > m_nominalPower)
        m_exportLimit = m_nominalPower;
    if (m_exportLimit < 0)
        m_exportLimit = 0;
}

void PowerControlSofarsolar::setExportLimit(double value)
{
    m_exportLimit = value;

    if (m_nominalPower == 0)
        return;

    if (m_exportLimit > m_nominalPower)
        m_exportLimit = m_nominalPower;
    if (m_exportLimit < 0)
        m_exportLimit = 0;

    m_limitRegister = m_exportLimit * 100 / m_nominalPower;
}

void PowerControlSofarsolar::setExportLimitEnable(bool value)
{
    if (value)
        m_controlRegister |= 0x01;
    else
        m_controlRegister &= 0xFFFE;
}

QVector<quint16> PowerControlSofarsolar::Registers()
{
    return QVector<quint16>{m_controlRegister, quint16(m_limitRegister)};
}

void PowerControlSofarsolar::setNominalPower(unsigned short value)
{
    if (value > 0)
        m_nominalPower = value;
    else
        m_nominalPower = 1;
}

bool PowerControlSofarsolar::exportLimitEnabled()
{
    return (bool)(m_controlRegister & 0x0001);
}

double PowerControlSofarsolar::exportLimit()
{
    return (unsigned short)(m_nominalPower * m_limitRegister / 100);
}

double PowerControlSofarsolar::exportLimitRate()
{
    return m_limitRegister;
}
