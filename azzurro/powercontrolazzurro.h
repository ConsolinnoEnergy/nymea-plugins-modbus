#ifndef POWERCONTROLAZZURRO_H
#define POWERCONTROLAZZURRO_H

/**
 * @brief Class for managing power control settings of Azzurro inverters.
 *
 * This class provides functionality to control and manage power limits and related settings
 * for Azzurro inverter devices. It handles both absolute and relative power limiting
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
class PowerControlAzzurro
{

public:
    PowerControlAzzurro();

    bool powerLimitEnabled();
    unsigned short absolutePowerLimit();
    double relativePowerLimit();
    unsigned long combinedRegisters();
    void setCombinedRegisters(unsigned long value);
    void setNominalPower(unsigned short value);
    unsigned short nominalPower();
    void setAbsolutePowerLimit(unsigned short value);
    void setPowerLimitEnable(bool value);

private:
    unsigned short m_nominalPower;
    unsigned short m_controlRegister;
    unsigned short m_limitRegister;
};
#endif