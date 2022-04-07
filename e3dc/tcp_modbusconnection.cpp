/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2022, nymea GmbH
* Contact: contact@nymea.io
*
* This fileDescriptor is part of nymea.
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


#include "tcp_modbusconnection.h"
#include "loggingcategories.h"

NYMEA_LOGGING_CATEGORY(dcTCP_ModbusConnection, "TCP_ModbusConnection")

TCP_ModbusConnection::TCP_ModbusConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent) :
    ModbusTCPMaster(hostAddress, port, parent),
    m_slaveId(slaveId)
{
    
}

float TCP_ModbusConnection::currentPower() const
{
    return m_currentPower;
}

void TCP_ModbusConnection::initialize()
{
    // No init registers defined. Nothing to be done and we are finished.
    emit initializationFinished();
}

void TCP_ModbusConnection::update()
{
    updateCurrentPower();
}

void TCP_ModbusConnection::updateCurrentPower()
{
    // Update registers from Inverter current Power
    qCDebug(dcTCP_ModbusConnection()) << "--> Read \"Inverter current Power\" register:" << 40068 << "size:" << 2;
    QModbusReply *reply = readCurrentPower();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"Inverter current Power\" register" << 40068 << "size:" << 2 << values;
                    float receivedCurrentPower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian) * 1.0 * pow(10, -1);
                    if (m_currentPower != receivedCurrentPower) {
                        m_currentPower = receivedCurrentPower;
                        emit currentPowerChanged(m_currentPower);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcTCP_ModbusConnection()) << "Modbus reply error occurred while updating \"Inverter current Power\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcTCP_ModbusConnection()) << "Error occurred while reading \"Inverter current Power\" registers from" << hostAddress().toString() << errorString();
    }
}

QModbusReply *TCP_ModbusConnection::readCurrentPower()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 40068, 2);
    return sendReadRequest(request, m_slaveId);
}

void TCP_ModbusConnection::verifyInitFinished()
{
    if (m_pendingInitReplies.isEmpty()) {
        qCDebug(dcTCP_ModbusConnection()) << "Initialization finished of TCP_ModbusConnection" << hostAddress().toString();
        emit initializationFinished();
    }
}

QDebug operator<<(QDebug debug, TCP_ModbusConnection *tCP_ModbusConnection)
{
    debug.nospace().noquote() << "TCP_ModbusConnection(" << tCP_ModbusConnection->hostAddress().toString() << ":" << tCP_ModbusConnection->port() << ")" << "\n";
    debug.nospace().noquote() << "    - Inverter current Power:" << tCP_ModbusConnection->currentPower() << " [kW]" << "\n";
    return debug.quote().space();
}

