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

void DiscoveryRtu::startDiscovery() {
    qCInfo(dcBroetje()) << "Discovery: Searching for Brötje heat pump on modbus RTU...";

    if (m_modbusRtuResource->modbusRtuMasters().isEmpty()) {
        qCWarning(dcBroetje()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
        return;
    }

    m_openReplies = 0;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->connected()) {
            // Broetje heat pumps have a Modbus Id in the range 100 - 115. Start searching with 100.
            tryConnect(master, 100);
        } else {
            qCWarning(dcBroetje()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }

    // This triggers if no master is connected.
    if (m_openReplies == 0) {
        emit discoveryFinished(true);
    }
}

QList<DiscoveryRtu::Result> DiscoveryRtu::discoveryResults() const {
    return m_discoveryResults;
}

void DiscoveryRtu::checkIfDone() {
    // This should not trigger. Have this check just in case.
    if (m_openReplies < 0) {
        m_openReplies = 0;
    }

    if (m_openReplies == 0) {
        emit discoveryFinished(true);
    }
}

void DiscoveryRtu::tryConnect(ModbusRtuMaster *master, quint16 modbusId) {
    qCDebug(dcBroetje()) << "Scanning modbus RTU master" << master->modbusUuid() << "with Modbus ID:" << modbusId;
    qCDebug(dcBroetje()) << "Trying to read register 277 (error) to test the connection.";

    m_openReplies++;
    ModbusRtuReply *reply = master->readHoldingRegister(modbusId, 277, 1);
    connect(reply, &ModbusRtuReply::finished, this, [=](){

        qCDebug(dcBroetje()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() < 1) {
            qCDebug(dcBroetje()) << "Error reading input register 277 (error). This is not a Brötje heat pump.";
        } else {
            // Test received value.
            quint16 errorValue = reply->result().first();
            if (errorValue == 0) {
                qCDebug(dcBroetje()) << "Recieved error value is" << errorValue << ". Value needs to be not 0, this is not a Brötje heat pump.";
            } else {
                qCDebug(dcBroetje()) << "Recieved error value is" << errorValue << ". Value needs to be not 0, seems ok.";

                Result result {master->modbusUuid(), modbusId, master->serialPort()};
                qCDebug(dcBroetje()) << "Found a Brötje heat pump on modbus RTU master" << master->modbusUuid() << "with Modbus ID:" << modbusId << ". Adding it to the list.";
                m_discoveryResults.append(result);
            }
        }

        if (modbusId < 115) {
            tryConnect(master, modbusId+1);
        }

        // Done checking these parameters. If there is something left to check, that was startet and +1 was added to m_openReplies.
        // Now we can do -1 to m_openReplies to signal this check is done. The late -1 is to ensure that m_openReplies is never 0 until everything is done.
        m_openReplies--;
        checkIfDone();
    });
}

