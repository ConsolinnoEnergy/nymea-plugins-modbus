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


#include "mennekesmodbustcpconnection.h"
#include "loggingcategories.h"

NYMEA_LOGGING_CATEGORY(dcMennekesModbusTcpConnection, "MennekesModbusTcpConnection")

MennekesModbusTcpConnection::MennekesModbusTcpConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent) :
    ModbusTCPMaster(hostAddress, port, parent),
    m_slaveId(slaveId)
{
    
}

quint32 MennekesModbusTcpConnection::firmware() const
{
    return m_firmware;
}

MennekesModbusTcpConnection::OCPPstatus MennekesModbusTcpConnection::ocppStatus() const
{
    return m_ocppStatus;
}

quint32 MennekesModbusTcpConnection::errorCode1() const
{
    return m_errorCode1;
}

quint32 MennekesModbusTcpConnection::errorCode2() const
{
    return m_errorCode2;
}

quint32 MennekesModbusTcpConnection::errorCode3() const
{
    return m_errorCode3;
}

quint32 MennekesModbusTcpConnection::errorCode4() const
{
    return m_errorCode4;
}

quint32 MennekesModbusTcpConnection::protocolVersion() const
{
    return m_protocolVersion;
}

quint16 MennekesModbusTcpConnection::vehicleState() const
{
    return m_vehicleState;
}

quint16 MennekesModbusTcpConnection::cpAvailable() const
{
    return m_cpAvailable;
}

QModbusReply *MennekesModbusTcpConnection::setCpAvailable(quint16 cpAvailable)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(cpAvailable);
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Write \"cpAvailable\" register:" << 124 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 124, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

quint16 MennekesModbusTcpConnection::safeCurrent() const
{
    return m_safeCurrent;
}

QModbusReply *MennekesModbusTcpConnection::setSafeCurrent(quint16 safeCurrent)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(safeCurrent);
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Write \"safeCurrent\" register:" << 131 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 131, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

quint16 MennekesModbusTcpConnection::commTimeout() const
{
    return m_commTimeout;
}

QModbusReply *MennekesModbusTcpConnection::setCommTimeout(quint16 commTimeout)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(commTimeout);
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Write \"commTimeout\" register:" << 132 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 132, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

quint32 MennekesModbusTcpConnection::meterEnergyL1() const
{
    return m_meterEnergyL1;
}

quint32 MennekesModbusTcpConnection::meterEnergyL2() const
{
    return m_meterEnergyL2;
}

quint32 MennekesModbusTcpConnection::meterEnergyL3() const
{
    return m_meterEnergyL3;
}

quint32 MennekesModbusTcpConnection::meterPowerL1() const
{
    return m_meterPowerL1;
}

quint32 MennekesModbusTcpConnection::meterPowerL2() const
{
    return m_meterPowerL2;
}

quint32 MennekesModbusTcpConnection::meterPowerL3() const
{
    return m_meterPowerL3;
}

quint32 MennekesModbusTcpConnection::meterCurrentL1() const
{
    return m_meterCurrentL1;
}

quint32 MennekesModbusTcpConnection::meterCurrentL2() const
{
    return m_meterCurrentL2;
}

quint32 MennekesModbusTcpConnection::meterCurrentL3() const
{
    return m_meterCurrentL3;
}

quint16 MennekesModbusTcpConnection::chargedEnergy() const
{
    return m_chargedEnergy;
}

quint16 MennekesModbusTcpConnection::currentLimitSetpointRead() const
{
    return m_currentLimitSetpointRead;
}

quint32 MennekesModbusTcpConnection::startTimehhmmss() const
{
    return m_startTimehhmmss;
}

quint16 MennekesModbusTcpConnection::chargeDuration() const
{
    return m_chargeDuration;
}

quint32 MennekesModbusTcpConnection::endTimehhmmss() const
{
    return m_endTimehhmmss;
}

quint16 MennekesModbusTcpConnection::currentLimitSetpoint() const
{
    return m_currentLimitSetpoint;
}

QModbusReply *MennekesModbusTcpConnection::setCurrentLimitSetpoint(quint16 currentLimitSetpoint)
{
    QVector<quint16> values = ModbusDataUtils::convertFromUInt16(currentLimitSetpoint);
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Write \"currentLimitSetpoint\" register:" << 1000 << "size:" << 1 << values;
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 1000, values.count());
    request.setValues(values);
    return sendWriteRequest(request, m_slaveId);
}

void MennekesModbusTcpConnection::initialize()
{
    // No init registers defined. Nothing to be done and we are finished.
    emit initializationFinished();
}

void MennekesModbusTcpConnection::update()
{
    updateFirmware();
    updateOcppStatus();
    updateErrorCode1();
    updateErrorCode2();
    updateErrorCode3();
    updateErrorCode4();
    updateProtocolVersion();
    updateVehicleState();
    updateCpAvailable();
    updateSafeCurrent();
    updateCommTimeout();
    updateMeterEnergyL1();
    updateMeterEnergyL2();
    updateMeterEnergyL3();
    updateMeterPowerL1();
    updateMeterPowerL2();
    updateMeterPowerL3();
    updateMeterCurrentL1();
    updateMeterCurrentL2();
    updateMeterCurrentL3();
    updateChargedEnergy();
    updateCurrentLimitSetpointRead();
    updateStartTimehhmmss();
    updateChargeDuration();
    updateEndTimehhmmss();
    updateCurrentLimitSetpoint();
}

void MennekesModbusTcpConnection::updateFirmware()
{
    // Update registers from firmware
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"firmware\" register:" << 100 << "size:" << 2;
    QModbusReply *reply = readFirmware();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"firmware\" register" << 100 << "size:" << 2 << values;
                    quint32 receivedFirmware = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_firmware != receivedFirmware) {
                        m_firmware = receivedFirmware;
                        emit firmwareChanged(m_firmware);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"firmware\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"firmware\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateOcppStatus()
{
    // Update registers from ocppStatus
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"ocppStatus\" register:" << 104 << "size:" << 1;
    QModbusReply *reply = readOcppStatus();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"ocppStatus\" register" << 104 << "size:" << 1 << values;
                    OCPPstatus receivedOcppStatus = static_cast<OCPPstatus>(ModbusDataUtils::convertToUInt16(values));
                    if (m_ocppStatus != receivedOcppStatus) {
                        m_ocppStatus = receivedOcppStatus;
                        emit ocppStatusChanged(m_ocppStatus);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"ocppStatus\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"ocppStatus\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateErrorCode1()
{
    // Update registers from errorCode1
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"errorCode1\" register:" << 105 << "size:" << 2;
    QModbusReply *reply = readErrorCode1();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"errorCode1\" register" << 105 << "size:" << 2 << values;
                    quint32 receivedErrorCode1 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_errorCode1 != receivedErrorCode1) {
                        m_errorCode1 = receivedErrorCode1;
                        emit errorCode1Changed(m_errorCode1);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"errorCode1\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"errorCode1\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateErrorCode2()
{
    // Update registers from errorCode2
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"errorCode2\" register:" << 107 << "size:" << 2;
    QModbusReply *reply = readErrorCode2();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"errorCode2\" register" << 107 << "size:" << 2 << values;
                    quint32 receivedErrorCode2 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_errorCode2 != receivedErrorCode2) {
                        m_errorCode2 = receivedErrorCode2;
                        emit errorCode2Changed(m_errorCode2);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"errorCode2\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"errorCode2\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateErrorCode3()
{
    // Update registers from errorCode3
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"errorCode3\" register:" << 109 << "size:" << 2;
    QModbusReply *reply = readErrorCode3();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"errorCode3\" register" << 109 << "size:" << 2 << values;
                    quint32 receivedErrorCode3 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_errorCode3 != receivedErrorCode3) {
                        m_errorCode3 = receivedErrorCode3;
                        emit errorCode3Changed(m_errorCode3);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"errorCode3\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"errorCode3\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateErrorCode4()
{
    // Update registers from errorCode4
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"errorCode4\" register:" << 111 << "size:" << 2;
    QModbusReply *reply = readErrorCode4();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"errorCode4\" register" << 111 << "size:" << 2 << values;
                    quint32 receivedErrorCode4 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_errorCode4 != receivedErrorCode4) {
                        m_errorCode4 = receivedErrorCode4;
                        emit errorCode4Changed(m_errorCode4);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"errorCode4\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"errorCode4\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateProtocolVersion()
{
    // Update registers from protocolVersion
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"protocolVersion\" register:" << 120 << "size:" << 2;
    QModbusReply *reply = readProtocolVersion();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"protocolVersion\" register" << 120 << "size:" << 2 << values;
                    quint32 receivedProtocolVersion = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_protocolVersion != receivedProtocolVersion) {
                        m_protocolVersion = receivedProtocolVersion;
                        emit protocolVersionChanged(m_protocolVersion);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"protocolVersion\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"protocolVersion\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateVehicleState()
{
    // Update registers from vehicleState
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"vehicleState\" register:" << 122 << "size:" << 1;
    QModbusReply *reply = readVehicleState();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"vehicleState\" register" << 122 << "size:" << 1 << values;
                    quint16 receivedVehicleState = ModbusDataUtils::convertToUInt16(values);
                    if (m_vehicleState != receivedVehicleState) {
                        m_vehicleState = receivedVehicleState;
                        emit vehicleStateChanged(m_vehicleState);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"vehicleState\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"vehicleState\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateCpAvailable()
{
    // Update registers from cpAvailable
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"cpAvailable\" register:" << 124 << "size:" << 1;
    QModbusReply *reply = readCpAvailable();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"cpAvailable\" register" << 124 << "size:" << 1 << values;
                    quint16 receivedCpAvailable = ModbusDataUtils::convertToUInt16(values);
                    if (m_cpAvailable != receivedCpAvailable) {
                        m_cpAvailable = receivedCpAvailable;
                        emit cpAvailableChanged(m_cpAvailable);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"cpAvailable\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"cpAvailable\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateSafeCurrent()
{
    // Update registers from safeCurrent
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"safeCurrent\" register:" << 131 << "size:" << 1;
    QModbusReply *reply = readSafeCurrent();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"safeCurrent\" register" << 131 << "size:" << 1 << values;
                    quint16 receivedSafeCurrent = ModbusDataUtils::convertToUInt16(values);
                    if (m_safeCurrent != receivedSafeCurrent) {
                        m_safeCurrent = receivedSafeCurrent;
                        emit safeCurrentChanged(m_safeCurrent);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"safeCurrent\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"safeCurrent\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateCommTimeout()
{
    // Update registers from commTimeout
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"commTimeout\" register:" << 132 << "size:" << 1;
    QModbusReply *reply = readCommTimeout();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"commTimeout\" register" << 132 << "size:" << 1 << values;
                    quint16 receivedCommTimeout = ModbusDataUtils::convertToUInt16(values);
                    if (m_commTimeout != receivedCommTimeout) {
                        m_commTimeout = receivedCommTimeout;
                        emit commTimeoutChanged(m_commTimeout);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"commTimeout\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"commTimeout\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterEnergyL1()
{
    // Update registers from meterEnergyL1
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterEnergyL1\" register:" << 200 << "size:" << 2;
    QModbusReply *reply = readMeterEnergyL1();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterEnergyL1\" register" << 200 << "size:" << 2 << values;
                    quint32 receivedMeterEnergyL1 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterEnergyL1 != receivedMeterEnergyL1) {
                        m_meterEnergyL1 = receivedMeterEnergyL1;
                        emit meterEnergyL1Changed(m_meterEnergyL1);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterEnergyL1\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterEnergyL1\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterEnergyL2()
{
    // Update registers from meterEnergyL2
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterEnergyL2\" register:" << 202 << "size:" << 2;
    QModbusReply *reply = readMeterEnergyL2();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterEnergyL2\" register" << 202 << "size:" << 2 << values;
                    quint32 receivedMeterEnergyL2 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterEnergyL2 != receivedMeterEnergyL2) {
                        m_meterEnergyL2 = receivedMeterEnergyL2;
                        emit meterEnergyL2Changed(m_meterEnergyL2);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterEnergyL2\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterEnergyL2\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterEnergyL3()
{
    // Update registers from meterEnergyL3
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterEnergyL3\" register:" << 204 << "size:" << 2;
    QModbusReply *reply = readMeterEnergyL3();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterEnergyL3\" register" << 204 << "size:" << 2 << values;
                    quint32 receivedMeterEnergyL3 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterEnergyL3 != receivedMeterEnergyL3) {
                        m_meterEnergyL3 = receivedMeterEnergyL3;
                        emit meterEnergyL3Changed(m_meterEnergyL3);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterEnergyL3\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterEnergyL3\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterPowerL1()
{
    // Update registers from meterPowerL1
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterPowerL1\" register:" << 206 << "size:" << 2;
    QModbusReply *reply = readMeterPowerL1();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterPowerL1\" register" << 206 << "size:" << 2 << values;
                    quint32 receivedMeterPowerL1 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterPowerL1 != receivedMeterPowerL1) {
                        m_meterPowerL1 = receivedMeterPowerL1;
                        emit meterPowerL1Changed(m_meterPowerL1);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterPowerL1\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterPowerL1\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterPowerL2()
{
    // Update registers from meterPowerL2
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterPowerL2\" register:" << 208 << "size:" << 2;
    QModbusReply *reply = readMeterPowerL2();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterPowerL2\" register" << 208 << "size:" << 2 << values;
                    quint32 receivedMeterPowerL2 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterPowerL2 != receivedMeterPowerL2) {
                        m_meterPowerL2 = receivedMeterPowerL2;
                        emit meterPowerL2Changed(m_meterPowerL2);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterPowerL2\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterPowerL2\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterPowerL3()
{
    // Update registers from meterPowerL3
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterPowerL3\" register:" << 210 << "size:" << 2;
    QModbusReply *reply = readMeterPowerL3();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterPowerL3\" register" << 210 << "size:" << 2 << values;
                    quint32 receivedMeterPowerL3 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterPowerL3 != receivedMeterPowerL3) {
                        m_meterPowerL3 = receivedMeterPowerL3;
                        emit meterPowerL3Changed(m_meterPowerL3);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterPowerL3\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterPowerL3\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterCurrentL1()
{
    // Update registers from meterCurrentL1
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterCurrentL1\" register:" << 212 << "size:" << 2;
    QModbusReply *reply = readMeterCurrentL1();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterCurrentL1\" register" << 212 << "size:" << 2 << values;
                    quint32 receivedMeterCurrentL1 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterCurrentL1 != receivedMeterCurrentL1) {
                        m_meterCurrentL1 = receivedMeterCurrentL1;
                        emit meterCurrentL1Changed(m_meterCurrentL1);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterCurrentL1\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterCurrentL1\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterCurrentL2()
{
    // Update registers from meterCurrentL2
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterCurrentL2\" register:" << 214 << "size:" << 2;
    QModbusReply *reply = readMeterCurrentL2();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterCurrentL2\" register" << 214 << "size:" << 2 << values;
                    quint32 receivedMeterCurrentL2 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterCurrentL2 != receivedMeterCurrentL2) {
                        m_meterCurrentL2 = receivedMeterCurrentL2;
                        emit meterCurrentL2Changed(m_meterCurrentL2);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterCurrentL2\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterCurrentL2\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateMeterCurrentL3()
{
    // Update registers from meterCurrentL3
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"meterCurrentL3\" register:" << 216 << "size:" << 2;
    QModbusReply *reply = readMeterCurrentL3();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"meterCurrentL3\" register" << 216 << "size:" << 2 << values;
                    quint32 receivedMeterCurrentL3 = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_meterCurrentL3 != receivedMeterCurrentL3) {
                        m_meterCurrentL3 = receivedMeterCurrentL3;
                        emit meterCurrentL3Changed(m_meterCurrentL3);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"meterCurrentL3\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"meterCurrentL3\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateChargedEnergy()
{
    // Update registers from chargedEnergy
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"chargedEnergy\" register:" << 705 << "size:" << 1;
    QModbusReply *reply = readChargedEnergy();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"chargedEnergy\" register" << 705 << "size:" << 1 << values;
                    quint16 receivedChargedEnergy = ModbusDataUtils::convertToUInt16(values);
                    if (m_chargedEnergy != receivedChargedEnergy) {
                        m_chargedEnergy = receivedChargedEnergy;
                        emit chargedEnergyChanged(m_chargedEnergy);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"chargedEnergy\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"chargedEnergy\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateCurrentLimitSetpointRead()
{
    // Update registers from currentLimitSetpointRead
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"currentLimitSetpointRead\" register:" << 706 << "size:" << 1;
    QModbusReply *reply = readCurrentLimitSetpointRead();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"currentLimitSetpointRead\" register" << 706 << "size:" << 1 << values;
                    quint16 receivedCurrentLimitSetpointRead = ModbusDataUtils::convertToUInt16(values);
                    if (m_currentLimitSetpointRead != receivedCurrentLimitSetpointRead) {
                        m_currentLimitSetpointRead = receivedCurrentLimitSetpointRead;
                        emit currentLimitSetpointReadChanged(m_currentLimitSetpointRead);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"currentLimitSetpointRead\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"currentLimitSetpointRead\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateStartTimehhmmss()
{
    // Update registers from startTimehhmmss
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"startTimehhmmss\" register:" << 707 << "size:" << 2;
    QModbusReply *reply = readStartTimehhmmss();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"startTimehhmmss\" register" << 707 << "size:" << 2 << values;
                    quint32 receivedStartTimehhmmss = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_startTimehhmmss != receivedStartTimehhmmss) {
                        m_startTimehhmmss = receivedStartTimehhmmss;
                        emit startTimehhmmssChanged(m_startTimehhmmss);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"startTimehhmmss\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"startTimehhmmss\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateChargeDuration()
{
    // Update registers from chargeDuration
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"chargeDuration\" register:" << 709 << "size:" << 1;
    QModbusReply *reply = readChargeDuration();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"chargeDuration\" register" << 709 << "size:" << 1 << values;
                    quint16 receivedChargeDuration = ModbusDataUtils::convertToUInt16(values);
                    if (m_chargeDuration != receivedChargeDuration) {
                        m_chargeDuration = receivedChargeDuration;
                        emit chargeDurationChanged(m_chargeDuration);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"chargeDuration\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"chargeDuration\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateEndTimehhmmss()
{
    // Update registers from endTimehhmmss
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"endTimehhmmss\" register:" << 710 << "size:" << 2;
    QModbusReply *reply = readEndTimehhmmss();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"endTimehhmmss\" register" << 710 << "size:" << 2 << values;
                    quint32 receivedEndTimehhmmss = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (m_endTimehhmmss != receivedEndTimehhmmss) {
                        m_endTimehhmmss = receivedEndTimehhmmss;
                        emit endTimehhmmssChanged(m_endTimehhmmss);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"endTimehhmmss\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"endTimehhmmss\" registers from" << hostAddress().toString() << errorString();
    }
}

void MennekesModbusTcpConnection::updateCurrentLimitSetpoint()
{
    // Update registers from currentLimitSetpoint
    qCDebug(dcMennekesModbusTcpConnection()) << "--> Read \"currentLimitSetpoint\" register:" << 1000 << "size:" << 1;
    QModbusReply *reply = readCurrentLimitSetpoint();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcMennekesModbusTcpConnection()) << "<-- Response from \"currentLimitSetpoint\" register" << 1000 << "size:" << 1 << values;
                    quint16 receivedCurrentLimitSetpoint = ModbusDataUtils::convertToUInt16(values);
                    if (m_currentLimitSetpoint != receivedCurrentLimitSetpoint) {
                        m_currentLimitSetpoint = receivedCurrentLimitSetpoint;
                        emit currentLimitSetpointChanged(m_currentLimitSetpoint);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcMennekesModbusTcpConnection()) << "Modbus reply error occurred while updating \"currentLimitSetpoint\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcMennekesModbusTcpConnection()) << "Error occurred while reading \"currentLimitSetpoint\" registers from" << hostAddress().toString() << errorString();
    }
}

QModbusReply *MennekesModbusTcpConnection::readFirmware()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 100, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readOcppStatus()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 104, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readErrorCode1()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 105, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readErrorCode2()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 107, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readErrorCode3()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 109, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readErrorCode4()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 111, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readProtocolVersion()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 120, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readVehicleState()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 122, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readCpAvailable()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 124, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readSafeCurrent()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 131, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readCommTimeout()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 132, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterEnergyL1()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 200, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterEnergyL2()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 202, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterEnergyL3()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 204, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterPowerL1()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 206, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterPowerL2()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 208, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterPowerL3()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 210, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterCurrentL1()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 212, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterCurrentL2()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 214, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readMeterCurrentL3()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 216, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readChargedEnergy()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 705, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readCurrentLimitSetpointRead()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 706, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readStartTimehhmmss()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 707, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readChargeDuration()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 709, 1);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readEndTimehhmmss()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 710, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *MennekesModbusTcpConnection::readCurrentLimitSetpoint()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 1000, 1);
    return sendReadRequest(request, m_slaveId);
}

void MennekesModbusTcpConnection::verifyInitFinished()
{
    if (m_pendingInitReplies.isEmpty()) {
        qCDebug(dcMennekesModbusTcpConnection()) << "Initialization finished of MennekesModbusTcpConnection" << hostAddress().toString();
        emit initializationFinished();
    }
}

QDebug operator<<(QDebug debug, MennekesModbusTcpConnection *mennekesModbusTcpConnection)
{
    debug.nospace().noquote() << "MennekesModbusTcpConnection(" << mennekesModbusTcpConnection->hostAddress().toString() << ":" << mennekesModbusTcpConnection->port() << ")" << "\n";
    debug.nospace().noquote() << "    - firmware:" << mennekesModbusTcpConnection->firmware() << "\n";
    debug.nospace().noquote() << "    - ocppStatus:" << mennekesModbusTcpConnection->ocppStatus() << "\n";
    debug.nospace().noquote() << "    - errorCode1:" << mennekesModbusTcpConnection->errorCode1() << "\n";
    debug.nospace().noquote() << "    - errorCode2:" << mennekesModbusTcpConnection->errorCode2() << "\n";
    debug.nospace().noquote() << "    - errorCode3:" << mennekesModbusTcpConnection->errorCode3() << "\n";
    debug.nospace().noquote() << "    - errorCode4:" << mennekesModbusTcpConnection->errorCode4() << "\n";
    debug.nospace().noquote() << "    - protocolVersion:" << mennekesModbusTcpConnection->protocolVersion() << "\n";
    debug.nospace().noquote() << "    - vehicleState:" << mennekesModbusTcpConnection->vehicleState() << "\n";
    debug.nospace().noquote() << "    - cpAvailable:" << mennekesModbusTcpConnection->cpAvailable() << "\n";
    debug.nospace().noquote() << "    - safeCurrent:" << mennekesModbusTcpConnection->safeCurrent() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - commTimeout:" << mennekesModbusTcpConnection->commTimeout() << " [Seconds]" << "\n";
    debug.nospace().noquote() << "    - meterEnergyL1:" << mennekesModbusTcpConnection->meterEnergyL1() << " [WattHour]" << "\n";
    debug.nospace().noquote() << "    - meterEnergyL2:" << mennekesModbusTcpConnection->meterEnergyL2() << " [WattHour]" << "\n";
    debug.nospace().noquote() << "    - meterEnergyL3:" << mennekesModbusTcpConnection->meterEnergyL3() << " [WattHour]" << "\n";
    debug.nospace().noquote() << "    - meterPowerL1:" << mennekesModbusTcpConnection->meterPowerL1() << " [Watt]" << "\n";
    debug.nospace().noquote() << "    - meterPowerL2:" << mennekesModbusTcpConnection->meterPowerL2() << " [Watt]" << "\n";
    debug.nospace().noquote() << "    - meterPowerL3:" << mennekesModbusTcpConnection->meterPowerL3() << " [Watt]" << "\n";
    debug.nospace().noquote() << "    - meterCurrentL1:" << mennekesModbusTcpConnection->meterCurrentL1() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - meterCurrentL2:" << mennekesModbusTcpConnection->meterCurrentL2() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - meterCurrentL3:" << mennekesModbusTcpConnection->meterCurrentL3() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - chargedEnergy:" << mennekesModbusTcpConnection->chargedEnergy() << " [WattHour]" << "\n";
    debug.nospace().noquote() << "    - currentLimitSetpointRead:" << mennekesModbusTcpConnection->currentLimitSetpointRead() << " [Ampere]" << "\n";
    debug.nospace().noquote() << "    - startTimehhmmss:" << mennekesModbusTcpConnection->startTimehhmmss() << "\n";
    debug.nospace().noquote() << "    - chargeDuration:" << mennekesModbusTcpConnection->chargeDuration() << " [Seconds]" << "\n";
    debug.nospace().noquote() << "    - endTimehhmmss:" << mennekesModbusTcpConnection->endTimehhmmss() << "\n";
    debug.nospace().noquote() << "    - currentLimitSetpoint:" << mennekesModbusTcpConnection->currentLimitSetpoint() << " [Ampere]" << "\n";
    return debug.quote().space();
}

