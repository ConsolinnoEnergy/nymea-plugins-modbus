/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#include "discoveryrtu.h"
#include "extern-plugininfo.h"

DiscoveryRtu::DiscoveryRtu(ModbusRtuHardwareResource *modbusRtuResource, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource}
{
}

void DiscoveryRtu::startDiscovery()
{
    qCInfo(dcSolax()) << "Discovery: Searching for Solax inverter on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::NoParity) {
            candidateMasters.append(master);
        }
    }

    if (candidateMasters.isEmpty()) {
        qCWarning(dcSolax()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            tryConnect(master, 1);
        } else {
            qCWarning(dcSolax()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
}

QList<DiscoveryRtu::Result> DiscoveryRtu::discoveryResults() const
{
    return m_discoveryResults;
}

void DiscoveryRtu::tryConnect(ModbusRtuMaster *master, quint16 modbusId)
{
    qCDebug(dcSolax()) << "Scanning modbus RTU master" << master->modbusUuid().toString() << "Slave ID:" << modbusId;

    // Get the "Inverter rated power" register. If this is a solax, only certain values can be in this register.
    ModbusRtuReply *reply = master->readHoldingRegister(modbusId, 186);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcSolax()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() == ModbusRtuReply::NoError && reply->result().length() > 0) {
            quint16 ratedPower = reply->result().first();

            // Smallest Solax inverter has 0.6 kW.
            bool powerIsInValidRange{false};
            if (ratedPower >= 600) {
                powerIsInValidRange = true;
            }

            // If this number is a power rating, the last two digits are zero.
            bool lastTwoDigitsAreZero{false};
            if ((ratedPower % 100) == 0) {
                lastTwoDigitsAreZero = true;
            }

            if (powerIsInValidRange && lastTwoDigitsAreZero) {
                qCDebug(dcSolax()) << QString("Rated power is %1").arg(ratedPower);
                Result result;
                result.modbusId = modbusId;
                result.powerRating = ratedPower;
                result.modbusRtuMasterId = master->modbusUuid();
                m_discoveryResults.append(result);
            } else {
                qCDebug(dcSolax()) << QString("The value in holding register 0xBA is %1, which does not seem to be a power rating. This is not a Solax inverter.").arg(ratedPower);
            }
        }
        if (modbusId < 20) {
            tryConnect(master, modbusId+1);
        } else {
            emit discoveryFinished(true);
        }
    });
}

