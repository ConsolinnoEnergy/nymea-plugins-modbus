#ifndef POWERCONTROLSOFARSOLAR_H
#define POWERCONTROLSOFARSOLAR_H

#include <QObject>
/**
 * @brief Class for managing power control settings of Sofar Solar inverters.
 *
 * This class provides functionality to control and manage power limits and related settings
 * for Sofar Solar inverter devices. It handles both absolute and relative power limiting
 * capabilities as well as the nominal power settings.
 *
 * @details The class manages three main aspects:
 * - Power limit enable/disable state
 * - Absolute power limit value
 * - Nominal power settings
 *
 * The power control is achieved through manipulation of control and limit registers,
 * which can be combined or separated as needed for the actual device communication.
 */
class PowerControlSofarsolar
{
public:
    PowerControlSofarsolar();

    bool activePowerLimitEnabled();
    unsigned short activePowerOutputLimit();
    double relativePowerLimit();
    unsigned long combinedRegisters();
    void setCombinedRegisters(unsigned long value);
    void setNominalPower(unsigned short value);
    unsigned short nominalPower();
    void setRelativePowerOutputLimit(unsigned short value);
    void setActivePowerOutputLimit(unsigned short value);
    void setActivePowerLimitEnable(bool value);
    QVector<quint16> getAllRegisters();

private:
    unsigned short m_nominalPower;
    unsigned short m_controlRegister;
    unsigned short m_limitRegister;
};
#endif