/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2021, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "lpcinterface.h"
#include "extern-plugininfo.h"

LpcInterface::LpcInterface(int gpioNumber1, QObject *parent) :
    QObject(parent),
    m_gpioNumber1(gpioNumber1)
{

}

bool LpcInterface::setLimitPowerConsumption(bool limitPowerConsumption)
{
    if (!isValid())
        return false;

    // Lambda uses an "EVU Freigabe", which means that the relais S1 shall be always switched on if LPC is inactive
    bool gpioSetting = !limitPowerConsumption;

    if (!m_gpio1->setValue(gpioSetting ? Gpio::ValueHigh : Gpio::ValueLow)) {
        qCWarning(dcLambda()) << "Could not switch GPIO 1 for setting LPC to " << limitPowerConsumption;
        return false;
    }   

    emit limitPowerConsumptionChanged(limitPowerConsumption);

    qCWarning(dcLambda()) << "Set GPIO 1 to " << gpioSetting;

    return true;
}

bool LpcInterface::setup(bool gpio1Enabled)
{
    m_gpio1 = setupGpio(m_gpioNumber1, gpio1Enabled);
    if (!m_gpio1) {
        qCWarning(dcLambda()) << "Failed to set up LPC interface gpio 1" << m_gpioNumber1;
        return false;
    }

    bool limitPowerConsumption = !gpio1Enabled;    
    emit limitPowerConsumptionChanged(limitPowerConsumption);

    return true;
}

bool LpcInterface::isValid() const
{
    return m_gpioNumber1 >= 0 && m_gpio1;
}

Gpio *LpcInterface::gpio1() const
{
    return m_gpio1;
}

Gpio *LpcInterface::setupGpio(int gpioNumber, bool initialValue)
{
    if (gpioNumber < 0) {
        qCWarning(dcLambda()) << "Invalid gpio number for setting up gpio" << gpioNumber;
        return nullptr;
    }

    // Create and configure gpio
    Gpio *gpio = new Gpio(gpioNumber, this);
    if (!gpio->exportGpio()) {
        qCWarning(dcLambda()) << "Could not export gpio" << gpioNumber;
        gpio->deleteLater();
        return nullptr;
    }

    if (!gpio->setDirection(Gpio::DirectionOutput)) {
        qCWarning(dcLambda()) << "Failed to configure gpio" << gpioNumber << "as output";
        gpio->deleteLater();
        return nullptr;
    }

    // Set the pin to the initial value
    if (!gpio->setValue(initialValue ? Gpio::ValueHigh : Gpio::ValueLow)) {
        qCWarning(dcLambda()) << "Failed to set initially value" << initialValue << "for gpio" << gpioNumber;
        gpio->deleteLater();
        return nullptr;
    }

    qCWarning(dcLambda()) << "Setup GPIO number " << gpioNumber << "with initial value " << initialValue;

    return gpio;
}
