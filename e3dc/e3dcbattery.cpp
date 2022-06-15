#include "e3dcbattery.h"

e3dcBattery::e3dcBattery()
{

}

float e3dcBattery::currentPower()
{
    return m_currentPower;
}

void e3dcBattery::setCurrentPower(float currentPower)
{
    m_currentPower = currentPower;
}

int e3dcBattery::SOC()
{
    return m_SOC;
}

void e3dcBattery::setSOC(int SOC)
{
    m_SOC = SOC;
}

int e3dcBattery::batteryID()
{
    return m_batteryID;
}

void e3dcBattery::setBatteryID(int ID)
{
    m_batteryID = ID;
}
