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
    qCInfo(dcABB()) << "Discovery: Searching for ABB Terra wallboxes on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->baudrate() == 19200 && master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::EvenParity) {
            candidateMasters.append(master);
        }
    }

    if (candidateMasters.isEmpty()) {
        qCWarning(dcABB()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            tryConnect(master, 1);
        } else {
            qCWarning(dcABB()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
}

QList<TerraRTUDiscovery::Result> TerraRTUDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void TerraRTUDiscovery::tryConnect(ModbusRtuMaster *master, quint16 slaveId)
{
    qCDebug(dcABB()) << "Scanning modbus RTU master" << master->modbusUuid() << "Slave ID:" << slaveId;

    ModbusRtuReply *reply = master->readInputRegister(slaveId, 4);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcABB()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() == ModbusRtuReply::NoError && reply->result().length() > 0) {
            quint16 version = reply->result().first();
            // TODO Versioncontrol not sure if this is necessary like this
            if (version >= 0x0100) {
                qCDebug(dcABB()) << QString("Version is 0x%1").arg(version, 0, 16);
                Result result {master->modbusUuid(), version, slaveId};
                m_discoveryResults.append(result);
            } else {
                qCDebug(dcABB()) << "Version must be at least 1.0.0 (0x0100)";
            }
        }
        if (slaveId < 20) {
            tryConnect(master, slaveId+1);
        } else {
            emit discoveryFinished(true);
        }
    });
}

