#include "e3dcwallbox.h"

e3dcWallbox::e3dcWallbox()
{

}

float e3dcWallbox::currentPower()
{
    return m_current_Power;
}

void e3dcWallbox::setCurrentPower(float currentPower)
{
    m_current_Power = currentPower;
}

int e3dcWallbox::wallboxID()
{
  return m_wallboxID;
}

void e3dcWallbox::setWallboxID(int ID)
{
  m_wallboxID = ID;
}
