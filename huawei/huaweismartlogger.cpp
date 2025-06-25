/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2025, nymea GmbH
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

#include "huaweismartlogger.h"

Q_DECLARE_LOGGING_CATEGORY(dcHuaweiSmartLoggerModbusTcpConnection)

HuaweiSmartLogger::HuaweiSmartLogger(const QHostAddress &hostAddress, uint port, quint16 meterSlaveId, QObject *parent)
    : HuaweiSmartLoggerModbusTcpConnection{hostAddress, port, 0, parent},
    m_meterSlaveId{meterSlaveId}
{
    // Note: the smart logger only responds to requests on slave ID 0 (broadcast), the meter itself can be
    // accessed using the connected modbus RTU slave ID, therefore we need to handle 2 modbus slave IDs with one modbus TCP connection

}

bool HuaweiSmartLogger::update()
{
    if (!m_modbusTcpMaster->connected())
        return false;

    if (!m_pendingUpdateReplies.isEmpty()) {
        qCDebug(dcHuaweiSmartLoggerModbusTcpConnection()) << "Tried to update but there are still some update replies pending. Waiting for them to be finished...";
        return true;
    }

    QModbusReply *reply = nullptr;

    // Read Total active output power of all inverters
    qCDebug(dcHuaweiSmartLoggerModbusTcpConnection()) << "--> Read \"Total active output power of all inverters\" register:" << 40525 << "size:" << 2;
    reply = readInverterTotalActivePower();
    if (!reply) {
        qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Error occurred while reading \"Total active output power of all inverters\" registers from" << m_modbusTcpMaster->hostAddress().toString() << m_modbusTcpMaster->errorString();
        return false;
    }

    if (reply->isFinished()) {
        reply->deleteLater(); // Broadcast reply returns immediately
        return false;
    }

    m_pendingUpdateReplies.append(reply);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, reply](){
        m_pendingUpdateReplies.removeAll(reply);
        handleModbusError(reply->error());
        if (reply->error() != QModbusDevice::NoError) {
            verifyUpdateFinished();
            return;
        }

        const QModbusDataUnit unit = reply->result();
        processInverterTotalActivePowerRegisterValues(unit.values());
        verifyUpdateFinished();
    });

    connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
        QModbusResponse response = reply->rawResult();
        if (reply->error() == QModbusDevice::ProtocolError && response.isException()) {
            qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Modbus reply error occurred while reading \"Total active output power of all inverters\" registers from" << m_modbusTcpMaster->hostAddress().toString() << error << reply->errorString() << ModbusDataUtils::exceptionCodeToString(response.exceptionCode());
        } else {
            qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Modbus reply error occurred while reading \"Total active output power of all inverters\" registers from" << m_modbusTcpMaster->hostAddress().toString() << error << reply->errorString();
        }
    });

    // Read Total energy yield produced by all inverters
    qCDebug(dcHuaweiSmartLoggerModbusTcpConnection()) << "--> Read \"Total energy yield produced by all inverters\" register:" << 40560 << "size:" << 2;
    reply = readInverterTotalEnergyProduced();
    if (!reply) {
        qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Error occurred while reading \"Total energy yield produced by all inverters\" registers from" << m_modbusTcpMaster->hostAddress().toString() << m_modbusTcpMaster->errorString();
        return false;
    }

    if (reply->isFinished()) {
        reply->deleteLater(); // Broadcast reply returns immediately
        return false;
    }

    m_pendingUpdateReplies.append(reply);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, reply](){
        m_pendingUpdateReplies.removeAll(reply);
        handleModbusError(reply->error());
        if (reply->error() != QModbusDevice::NoError) {
            verifyUpdateFinished();
            return;
        }

        const QModbusDataUnit unit = reply->result();
        processInverterTotalEnergyProducedRegisterValues(unit.values());
        verifyUpdateFinished();
    });

    connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
        QModbusResponse response = reply->rawResult();
        if (reply->error() == QModbusDevice::ProtocolError && response.isException()) {
            qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Modbus reply error occurred while reading \"Total energy yield produced by all inverters\" registers from" << m_modbusTcpMaster->hostAddress().toString() << error << reply->errorString() << ModbusDataUtils::exceptionCodeToString(response.exceptionCode());
        } else {
            qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Modbus reply error occurred while reading \"Total energy yield produced by all inverters\" registers from" << m_modbusTcpMaster->hostAddress().toString() << error << reply->errorString();
        }
    });

    // Read meterData

    // Temporary set the slave ID to the configured meter id, send the request and reset to broadcast...
    m_slaveId = m_meterSlaveId;
    reply = readBlockMeterData();
    m_slaveId = 0;
    qCDebug(dcHuaweiSmartLoggerModbusTcpConnection()) << "--> Read block \"meterData\" registers from:" << 32260 << "size:" << 105;
    if (!reply) {
        qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Error occurred while reading block \"meterData\" registers";
        return false;
    }

    if (reply->isFinished()) {
        reply->deleteLater(); // Broadcast reply returns immediately
        return false;
    }

    m_pendingUpdateReplies.append(reply);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, reply](){
        m_pendingUpdateReplies.removeAll(reply);
        handleModbusError(reply->error());
        if (reply->error() != QModbusDevice::NoError) {
            verifyUpdateFinished();
            return;
        }

        const QModbusDataUnit unit = reply->result();
        const QVector<quint16> blockValues = unit.values();
        processBlockMeterDataRegisterValues(blockValues);
        verifyUpdateFinished();
    });

    connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
        QModbusResponse response = reply->rawResult();
        if (reply->error() == QModbusDevice::ProtocolError && response.isException()) {
            qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Modbus reply error occurred while updating block \"meterData\" registers" << error << reply->errorString() << ModbusDataUtils::exceptionCodeToString(response.exceptionCode());
        } else {
            qCWarning(dcHuaweiSmartLoggerModbusTcpConnection()) << "Modbus reply error occurred while updating block \"meterData\" registers" << error << reply->errorString();
        }
    });

    return true;
}
