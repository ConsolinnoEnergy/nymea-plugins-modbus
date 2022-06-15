#include "e3dcinverter.h"

e3dcInverter::e3dcInverter()
{

}

float e3dcInverter::currentPower()
{
    return m_current_Power;
}

void e3dcInverter::setCurrentPower(float currentPower)
{
    m_current_Power = currentPower;
}

int e3dcInverter::inverterID()
{
    return m_inverterID;
}

void e3dcInverter::setInverterID(int ID)
{
    m_inverterID = ID;
}

float e3dcInverter::neworkPointPower()
{
    return m_network_Point_Power;
}

void e3dcInverter::setNetworkPointPower(float networkPointPower)
{
    m_network_Point_Power = networkPointPower;
}
