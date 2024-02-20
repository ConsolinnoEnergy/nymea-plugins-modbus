/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "terraRTUdiscovery.h"
#include "extern-plugininfo.h"

TerraRTUDiscovery::TerraRTUDiscovery(ModbusRtuHardwareResource *modbusRtuResource, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource}
{
}

void TerraRTUDiscovery::startDiscovery()
{
    qCInfo(dcAbb()) << "Discovery: Searching for ABB Terra wallboxes on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->baudrate() == 19200 && master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::EvenParity) {
            candidateMasters.append(master);
        }
    }

    if (candidateMasters.isEmpty()) {
        // TODO: add paramters of RTU Master
        qCWarning(dcAbb()) << "No usable modbus RTU master found.";
        qCWarning(dcAbb()) << "Modbus Parameters should be set to: Baudrate - 19200 | Databits - 8 | Stopbits - 1 | Parity - Even";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            tryConnect(master, 1);
        } else {
            qCWarning(dcAbb()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
}

QList<TerraRTUDiscovery::Result> TerraRTUDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void TerraRTUDiscovery::tryConnect(ModbusRtuMaster *master, quint16 slaveId)
{
    qCDebug(dcAbb()) << "Scanning modbus RTU master" << master->modbusUuid() << "Slave ID:" << slaveId;

    // TODO: choose different register to test connection
    ModbusRtuReply *reply = master->readInputRegister(slaveId, 16416);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcAbb()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() == ModbusRtuReply::NoError && reply->result().length() > 0) {
            quint16 communicationTimeout = reply->result().first();
            // TODO Versioncontrol not sure if this is necessary like this
            if (communicationTimeout > 0) {
                qCDebug(dcAbb()) << QString("Communication Timeout is 0x%1").arg(communicationTimeout, 0, 16);
                Result result {master->modbusUuid(), communicationTimeout, slaveId};
                m_discoveryResults.append(result);
            } else {
                qCDebug(dcAbb()) << "Communication Timeout must be greater than 0";
            }
        }
        if (slaveId < 20) {
            tryConnect(master, slaveId+1);
        } else {
            emit discoveryFinished(true);
        }
    });
}

