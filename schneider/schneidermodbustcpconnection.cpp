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


#include "schneidermodbustcpconnection.h"
#include "loggingcategories.h"

NYMEA_LOGGING_CATEGORY(dcSchneiderModbusTcpConnection, "SchneiderModbusTcpConnection")

SchneiderModbusTcpConnection::SchneiderModbusTcpConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent) :
    ModbusTCPMaster(hostAddress, port, parent),
    m_slaveId(slaveId)
{
    
}

SchneiderModbusTcpConnection::CPWState SchneiderModbusTcpConnection::cpwState() const
{
    return m_cpwState;
}

SchneiderModbusTcpConnection::LastChargeStatus SchneiderModbusTcpConnection::lastChargeStatus() const
{
    return m_lastChargeStatus;
}

quint16 SchneiderModbusTcpConnection::remoteCommandStatus() const
{
    return m_remoteCommandStatus;
}

quint32 SchneiderModbusTcpConnection::errorStatus() const
{
    return m_errorStatus;
}

quint32 SchneiderModbusTcpConnection::chargeTime() const
{
    return m_chargeTime;
}

SchneiderModbusTcpConnection::RemoteCommand SchneiderModbusTcpConnection::remoteCommand() const
{
    return m_remoteCommand;
}

QModbusReply *SchneiderModbusTcpConnection::setRemoteCommand(RemoteCommand remoteCommand)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(static_cast<quint16>(remoteCommand));
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Write \"remoteCommand\" register:" << 150 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 150, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

quint16 SchneiderModbusTcpConnection::maxIntensitySocket() const
{
    return m_maxIntensitySocket;
}

QModbusReply *SchneiderModbusTcpConnection::setMaxIntensitySocket(quint16 maxIntensitySocket)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(maxIntensitySocket);
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Write \"maxIntensitySocket\" register:" << 301 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 301, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

quint16 SchneiderModbusTcpConnection::maxIntensityCluster() const
{
    return m_maxIntensityCluster;
}

QModbusReply *SchneiderModbusTcpConnection::setMaxIntensityCluster(quint16 maxIntensityCluster)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(maxIntensityCluster);
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Write \"maxIntensityCluster\" register:" << 310 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 310, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

float SchneiderModbusTcpConnection::stationIntensityPhaseX() const
{
    return m_stationIntensityPhaseX;
}

float SchneiderModbusTcpConnection::stationIntensityPhase2() const
{
    return m_stationIntensityPhase2;
}

float SchneiderModbusTcpConnection::stationIntensityPhase3() const
{
    return m_stationIntensityPhase3;
}

quint32 SchneiderModbusTcpConnection::stationEnergy() const
{
    return m_stationEnergy;
}

float SchneiderModbusTcpConnection::stationPowerTotal() const
{
    return m_stationPowerTotal;
}

quint16 SchneiderModbusTcpConnection::remoteControllerLifeBit() const
{
    return m_remoteControllerLifeBit;
}

QModbusReply *SchneiderModbusTcpConnection::setRemoteControllerLifeBit(quint16 remoteControllerLifeBit)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(remoteControllerLifeBit);
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Write \"remoteControllerLifeBit\" register:" << 932 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 932, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

quint16 SchneiderModbusTcpConnection::degradedMode() const
{
    return m_degradedMode;
}

quint32 SchneiderModbusTcpConnection::sessionTime() const
{
    return m_sessionTime;
}

void SchneiderModbusTcpConnection::initialize()
{
    // No init registers defined. Nothing to be done and we are finished.
    emit initializationFinished();
}

void SchneiderModbusTcpConnection::update()
{
    updateCpwState();
    updateLastChargeStatus();
    updateRemoteCommandStatus();
    updateErrorStatus();
    updateChargeTime();
    updateRemoteCommand();
    updateMaxIntensitySocket();
    updateMaxIntensityCluster();
    updateStationIntensityPhaseX();
    updateStationIntensityPhase2();
    updateStationIntensityPhase3();
    updateStationEnergy();
    updateStationPowerTotal();
    updateRemoteControllerLifeBit();
    updateDegradedMode();
    updateSessionTime();
}

void SchneiderModbusTcpConnection::updateCpwState()
{
    // Update registers from cpwState
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"cpwState\" register:" << 6 << "size:" << 1;
    QModbusReply *reply = readCpwState();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"cpwState\" register" << 6 << "size:" << 1 << values;
                    CPWState receivedCpwState = static_cast<CPWState>(ModbusDataUtils::convertToUInt16(values));
                    if (m_cpwState != receivedCpwState) {
                        m_cpwState = receivedCpwState;
                        emit cpwStateChanged(m_cpwState);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"cpwState\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"cpwState\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateLastChargeStatus()
{
    // Update registers from lastChargeStatus
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"lastChargeStatus\" register:" << 9 << "size:" << 1;
    QModbusReply *reply = readLastChargeStatus();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"lastChargeStatus\" register" << 9 << "size:" << 1 << values;
                    LastChargeStatus receivedLastChargeStatus = static_cast<LastChargeStatus>(ModbusDataUtils::convertToUInt16(values));
                    if (m_lastChargeStatus != receivedLastChargeStatus) {
                        m_lastChargeStatus = receivedLastChargeStatus;
                        emit lastChargeStatusChanged(m_lastChargeStatus);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"lastChargeStatus\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"lastChargeStatus\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateRemoteCommandStatus()
{
    // Update registers from remoteCommandStatus
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"remoteCommandStatus\" register:" << 20 << "size:" << 1;
    QModbusReply *reply = readRemoteCommandStatus();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"remoteCommandStatus\" register" << 20 << "size:" << 1 << values;
                    quint16 receivedRemoteCommandStatus = ModbusDataUtils::convertToUInt16(values);
                    if (m_remoteCommandStatus != receivedRemoteCommandStatus) {
                        m_remoteCommandStatus = receivedRemoteCommandStatus;
                        emit remoteCommandStatusChanged(m_remoteCommandStatus);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"remoteCommandStatus\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"remoteCommandStatus\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateErrorStatus()
{
    // Update registers from errorStatus
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"errorStatus\" register:" << 23 << "size:" << 2;
    QModbusReply *reply = readErrorStatus();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"errorStatus\" register" << 23 << "size:" << 2 << values;
                    quint32 receivedErrorStatus = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_errorStatus != receivedErrorStatus) {
                        m_errorStatus = receivedErrorStatus;
                        emit errorStatusChanged(m_errorStatus);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"errorStatus\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"errorStatus\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateChargeTime()
{
    // Update registers from chargeTime
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"chargeTime\" register:" << 30 << "size:" << 2;
    QModbusReply *reply = readChargeTime();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"chargeTime\" register" << 30 << "size:" << 2 << values;
                    quint32 receivedChargeTime = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_chargeTime != receivedChargeTime) {
                        m_chargeTime = receivedChargeTime;
                        emit chargeTimeChanged(m_chargeTime);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"chargeTime\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"chargeTime\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateRemoteCommand()
{
    // Update registers from remoteCommand
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"remoteCommand\" register:" << 150 << "size:" << 1;
    QModbusReply *reply = readRemoteCommand();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"remoteCommand\" register" << 150 << "size:" << 1 << values;
                    RemoteCommand receivedRemoteCommand = static_cast<RemoteCommand>(ModbusDataUtils::convertToUInt16(values));
                    if (m_remoteCommand != receivedRemoteCommand) {
                        m_remoteCommand = receivedRemoteCommand;
                        emit remoteCommandChanged(m_remoteCommand);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"remoteCommand\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"remoteCommand\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateMaxIntensitySocket()
{
    // Update registers from maxIntensitySocket
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"maxIntensitySocket\" register:" << 301 << "size:" << 1;
    QModbusReply *reply = readMaxIntensitySocket();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"maxIntensitySocket\" register" << 301 << "size:" << 1 << values;
                    quint16 receivedMaxIntensitySocket = ModbusDataUtils::convertToUInt16(values);
                    if (m_maxIntensitySocket != receivedMaxIntensitySocket) {
                        m_maxIntensitySocket = receivedMaxIntensitySocket;
                        emit maxIntensitySocketChanged(m_maxIntensitySocket);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"maxIntensitySocket\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"maxIntensitySocket\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateMaxIntensityCluster()
{
    // Update registers from maxIntensityCluster
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"maxIntensityCluster\" register:" << 310 << "size:" << 1;
    QModbusReply *reply = readMaxIntensityCluster();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"maxIntensityCluster\" register" << 310 << "size:" << 1 << values;
                    quint16 receivedMaxIntensityCluster = ModbusDataUtils::convertToUInt16(values);
                    if (m_maxIntensityCluster != receivedMaxIntensityCluster) {
                        m_maxIntensityCluster = receivedMaxIntensityCluster;
                        emit maxIntensityClusterChanged(m_maxIntensityCluster);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"maxIntensityCluster\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"maxIntensityCluster\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateStationIntensityPhaseX()
{
    // Update registers from stationIntensityPhaseX
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"stationIntensityPhaseX\" register:" << 350 << "size:" << 2;
    QModbusReply *reply = readStationIntensityPhaseX();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"stationIntensityPhaseX\" register" << 350 << "size:" << 2 << values;
                    float receivedStationIntensityPhaseX = ModbusDataUtils::convertToFloat32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_stationIntensityPhaseX != receivedStationIntensityPhaseX) {
                        m_stationIntensityPhaseX = receivedStationIntensityPhaseX;
                        emit stationIntensityPhaseXChanged(m_stationIntensityPhaseX);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"stationIntensityPhaseX\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"stationIntensityPhaseX\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateStationIntensityPhase2()
{
    // Update registers from stationIntensityPhase2
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"stationIntensityPhase2\" register:" << 352 << "size:" << 2;
    QModbusReply *reply = readStationIntensityPhase2();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"stationIntensityPhase2\" register" << 352 << "size:" << 2 << values;
                    float receivedStationIntensityPhase2 = ModbusDataUtils::convertToFloat32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_stationIntensityPhase2 != receivedStationIntensityPhase2) {
                        m_stationIntensityPhase2 = receivedStationIntensityPhase2;
                        emit stationIntensityPhase2Changed(m_stationIntensityPhase2);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"stationIntensityPhase2\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"stationIntensityPhase2\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateStationIntensityPhase3()
{
    // Update registers from stationIntensityPhase3
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"stationIntensityPhase3\" register:" << 354 << "size:" << 2;
    QModbusReply *reply = readStationIntensityPhase3();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"stationIntensityPhase3\" register" << 354 << "size:" << 2 << values;
                    float receivedStationIntensityPhase3 = ModbusDataUtils::convertToFloat32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_stationIntensityPhase3 != receivedStationIntensityPhase3) {
                        m_stationIntensityPhase3 = receivedStationIntensityPhase3;
                        emit stationIntensityPhase3Changed(m_stationIntensityPhase3);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"stationIntensityPhase3\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"stationIntensityPhase3\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateStationEnergy()
{
    // Update registers from stationEnergy
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"stationEnergy\" register:" << 356 << "size:" << 2;
    QModbusReply *reply = readStationEnergy();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"stationEnergy\" register" << 356 << "size:" << 2 << values;
                    quint32 receivedStationEnergy = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_stationEnergy != receivedStationEnergy) {
                        m_stationEnergy = receivedStationEnergy;
                        emit stationEnergyChanged(m_stationEnergy);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"stationEnergy\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"stationEnergy\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateStationPowerTotal()
{
    // Update registers from stationPowerTotal
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"stationPowerTotal\" register:" << 358 << "size:" << 2;
    QModbusReply *reply = readStationPowerTotal();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"stationPowerTotal\" register" << 358 << "size:" << 2 << values;
                    float receivedStationPowerTotal = ModbusDataUtils::convertToFloat32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_stationPowerTotal != receivedStationPowerTotal) {
                        m_stationPowerTotal = receivedStationPowerTotal;
                        emit stationPowerTotalChanged(m_stationPowerTotal);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"stationPowerTotal\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"stationPowerTotal\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateRemoteControllerLifeBit()
{
    // Update registers from remoteControllerLifeBit
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"remoteControllerLifeBit\" register:" << 932 << "size:" << 1;
    QModbusReply *reply = readRemoteControllerLifeBit();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"remoteControllerLifeBit\" register" << 932 << "size:" << 1 << values;
                    quint16 receivedRemoteControllerLifeBit = ModbusDataUtils::convertToUInt16(values);
                    if (m_remoteControllerLifeBit != receivedRemoteControllerLifeBit) {
                        m_remoteControllerLifeBit = receivedRemoteControllerLifeBit;
                        emit remoteControllerLifeBitChanged(m_remoteControllerLifeBit);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"remoteControllerLifeBit\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"remoteControllerLifeBit\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateDegradedMode()
{
    // Update registers from degradedMode
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"degradedMode\" register:" << 933 << "size:" << 1;
    QModbusReply *reply = readDegradedMode();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"degradedMode\" register" << 933 << "size:" << 1 << values;
                    quint16 receivedDegradedMode = ModbusDataUtils::convertToUInt16(values);
                    if (m_degradedMode != receivedDegradedMode) {
                        m_degradedMode = receivedDegradedMode;
                        emit degradedModeChanged(m_degradedMode);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"degradedMode\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"degradedMode\" registers from" << hostAddress().toString() << errorString();
    }
}

void SchneiderModbusTcpConnection::updateSessionTime()
{
    // Update registers from sessionTime
    qCDebug(dcSchneiderModbusTcpConnection()) << "--> Read \"sessionTime\" register:" << 2004 << "size:" << 2;
    QModbusReply *reply = readSessionTime();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcSchneiderModbusTcpConnection()) << "<-- Response from \"sessionTime\" register" << 2004 << "size:" << 2 << values;
                    quint32 receivedSessionTime = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_sessionTime != receivedSessionTime) {
                        m_sessionTime = receivedSessionTime;
                        emit sessionTimeChanged(m_sessionTime);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcSchneiderModbusTcpConnection()) << "Modbus reply error occurred while updating \"sessionTime\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcSchneiderModbusTcpConnection()) << "Error occurred while reading \"sessionTime\" registers from" << hostAddress().toString() << errorString();
    }
}

QModbusReply *SchneiderModbusTcpConnection::readCpwState()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 6, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readLastChargeStatus()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 9, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readRemoteCommandStatus()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 20, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readErrorStatus()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 23, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readChargeTime()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 30, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readRemoteCommand()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 150, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readMaxIntensitySocket()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 301, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readMaxIntensityCluster()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 310, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readStationIntensityPhaseX()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 350, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readStationIntensityPhase2()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 352, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readStationIntensityPhase3()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 354, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readStationEnergy()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 356, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readStationPowerTotal()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 358, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readRemoteControllerLifeBit()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 932, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readDegradedMode()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 933, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *SchneiderModbusTcpConnection::readSessionTime()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 2004, 2);
    return sendReadRequest(request, m_slaveId);
}

void SchneiderModbusTcpConnection::verifyInitFinished()
{
    if (m_pendingInitReplies.isEmpty()) {
        qCDebug(dcSchneiderModbusTcpConnection()) << "Initialization finished of SchneiderModbusTcpConnection" << hostAddress().toString();
        emit initializationFinished();
    }
}

QDebug operator<<(QDebug debug, SchneiderModbusTcpConnection *schneiderModbusTcpConnection)
{
    debug.nospace().noquote() << "SchneiderModbusTcpConnection(" << schneiderModbusTcpConnection->hostAddress().toString() << ":" << schneiderModbusTcpConnection->port() << ")" << "\n";
    debug.nospace().noquote() << "    - cpwState:" << schneiderModbusTcpConnection->cpwState() << "\n";
    debug.nospace().noquote() << "    - lastChargeStatus:" << schneiderModbusTcpConnection->lastChargeStatus() << "\n";
    debug.nospace().noquote() << "    - remoteCommandStatus:" << schneiderModbusTcpConnection->remoteCommandStatus() << "\n";
    debug.nospace().noquote() << "    - errorStatus:" << schneiderModbusTcpConnection->errorStatus() << "\n";
    debug.nospace().noquote() << "    - chargeTime:" << schneiderModbusTcpConnection->chargeTime() << "\n";
    debug.nospace().noquote() << "    - remoteCommand:" << schneiderModbusTcpConnection->remoteCommand() << "\n";
    debug.nospace().noquote() << "    - maxIntensitySocket:" << schneiderModbusTcpConnection->maxIntensitySocket() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - maxIntensityCluster:" << schneiderModbusTcpConnection->maxIntensityCluster() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - stationIntensityPhaseX:" << schneiderModbusTcpConnection->stationIntensityPhaseX() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - stationIntensityPhase2:" << schneiderModbusTcpConnection->stationIntensityPhase2() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - stationIntensityPhase3:" << schneiderModbusTcpConnection->stationIntensityPhase3() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - stationEnergy:" << schneiderModbusTcpConnection->stationEnergy() << " [WattHour]" << "\n";
    debug.nospace().noquote() << "    - stationPowerTotal:" << schneiderModbusTcpConnection->stationPowerTotal() << " [KiloWatt]" << "\n";
    debug.nospace().noquote() << "    - remoteControllerLifeBit:" << schneiderModbusTcpConnection->remoteControllerLifeBit() << "\n";
    debug.nospace().noquote() << "    - degradedMode:" << schneiderModbusTcpConnection->degradedMode() << "\n";
    debug.nospace().noquote() << "    - sessionTime:" << schneiderModbusTcpConnection->sessionTime() << " [Seconds]" << "\n";
    return debug.quote().space();
}

