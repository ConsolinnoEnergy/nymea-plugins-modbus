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

#include "huaweifusionsolar.h"
#include "extern-plugininfo.h"
#include "loggingcategories.h"

NYMEA_LOGGING_CATEGORY(dcHuaweiFusionSolar, "HuaweiFusionSolar")

HuaweiFusionSolar::HuaweiFusionSolar(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent) :
    HuaweiModbusTcpConnection(hostAddress, port, slaveId, parent)
{

}

void HuaweiFusionSolar::initialize()
{
    // No init registers defined. Nothing to be done and we are finished.
    emit initializationFinished();
}

void HuaweiFusionSolar::update()
{
    // Make sure there is not an update still running
    if (!m_registersQueue.isEmpty())
        return;

    // Add the requests
    m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterInverterActivePower);
    m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterInverterDeviceStatus);
    m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterInverterEnergyProduced);
    m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterLunaBattery1Status);
    if (m_battery1Available) {
        m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterLunaBattery1Power);
        m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterLunaBattery1Soc);
    }
    m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterPowerMeterActivePower);
    m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterLunaBattery2Status);
    if (m_battery2Available) {
        m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterLunaBattery2Power);
        m_registersQueue.enqueue(HuaweiModbusTcpConnection::RegisterLunaBattery2Soc);
    }

    // Note: since huawei can only process one request at the time, we need to queue the requests

    m_currentRegisterRequest = -1;
    readNextRegister();
}

void HuaweiFusionSolar::readNextRegister()
{
    // Check if currently a reply is pending
    if (m_currentRegisterRequest >= 0)
        return;

    // Check if there is still a register to read
    if (m_registersQueue.isEmpty())
        return;

    m_currentRegisterRequest = m_registersQueue.dequeue();

    switch (m_currentRegisterRequest) {
    case HuaweiModbusTcpConnection::RegisterInverterActivePower: {
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Inverter active power\" register:" << 32080 << "size:" << 2;
        QModbusReply *reply = readInverterActivePower();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Inverter active power\" register" << 32080 << "size:" << 2 << values;
                        float receivedInverterActivePower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian) * 1.0 * pow(10, -3);
                        if (m_inverterActivePower != receivedInverterActivePower) {
                            m_inverterActivePower = receivedInverterActivePower;
                            emit inverterActivePowerChanged(m_inverterActivePower);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Inverter active power\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Inverter active power\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterInverterDeviceStatus: {
        // Update registers from Inverter device status
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Inverter device status\" register:" << 32089 << "size:" << 1;
        QModbusReply *reply = readInverterDeviceStatus();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Inverter device status\" register" << 32089 << "size:" << 1 << values;
                        InverterDeviceStatus receivedInverterDeviceStatus = static_cast<InverterDeviceStatus>(ModbusDataUtils::convertToUInt16(values));
                        qCDebug(dcHuaweiFusionSolar()) << "Inverter status" << receivedInverterDeviceStatus;
                        if (m_inverterDeviceStatus != receivedInverterDeviceStatus) {
                            m_inverterDeviceStatus = receivedInverterDeviceStatus;
                            emit inverterDeviceStatusChanged(m_inverterDeviceStatus);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Inverter device status\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Inverter device status\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterInverterEnergyProduced: {
        // Update registers from Inverter energy produced
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Inverter energy produced\" register:" << 32106 << "size:" << 2;
        QModbusReply *reply = readInverterEnergyProduced();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Inverter energy produced\" register" << 32106 << "size:" << 2 << values;
                        float receivedInverterEnergyProduced = ModbusDataUtils::convertToUInt32(values, ModbusDataUtils::ByteOrderBigEndian) * 1.0 * pow(10, -2);
                        if (m_inverterEnergyProduced != receivedInverterEnergyProduced) {
                            m_inverterEnergyProduced = receivedInverterEnergyProduced;
                            emit inverterEnergyProducedChanged(m_inverterEnergyProduced);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Inverter energy produced\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Inverter energy produced\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterLunaBattery1Status: {
        // Update registers from Luna 2000 Battery 1 status
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Luna 2000 Battery 1 status\" register:" << 37000 << "size:" << 1;
        QModbusReply *reply = readLunaBattery1Status();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Luna 2000 Battery 1 status\" register" << 37000 << "size:" << 1 << values;
                        BatteryDeviceStatus receivedLunaBattery1Status = static_cast<BatteryDeviceStatus>(ModbusDataUtils::convertToUInt16(values));
                        qCDebug(dcHuaweiFusionSolar()) << "Battery 1 status" << receivedLunaBattery1Status;
                        if (receivedLunaBattery1Status == BatteryDeviceStatusOffline) {
                            m_battery1Available = false;
                        } else {
                            m_battery1Available = true;
                        }

                        if (m_lunaBattery1Status != receivedLunaBattery1Status) {
                            m_lunaBattery1Status = receivedLunaBattery1Status;
                            emit lunaBattery1StatusChanged(m_lunaBattery1Status);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Luna 2000 Battery 1 status\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Luna 2000 Battery 1 status\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterLunaBattery1Power: {
        // Update registers from Luna 2000 Battery 1 power
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Luna 2000 Battery 1 power\" register:" << 37001 << "size:" << 2;
        QModbusReply *reply = readLunaBattery1Power();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Luna 2000 Battery 1 power\" register" << 37001 << "size:" << 2 << values;
                        qint32 receivedLunaBattery1Power = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                        if (m_lunaBattery1Power != receivedLunaBattery1Power) {
                            m_lunaBattery1Power = receivedLunaBattery1Power;
                            emit lunaBattery1PowerChanged(m_lunaBattery1Power);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Luna 2000 Battery 1 power\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Luna 2000 Battery 1 power\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterLunaBattery1Soc: {
        // Update registers from Luna 2000 Battery 1 state of charge
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Luna 2000 Battery 1 state of charge\" register:" << 37004 << "size:" << 1;
        QModbusReply *reply = readLunaBattery1Soc();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Luna 2000 Battery 1 state of charge\" register" << 37004 << "size:" << 1 << values;
                        float receivedLunaBattery1Soc = ModbusDataUtils::convertToUInt16(values) * 1.0 * pow(10, -1);
                        if (m_lunaBattery1Soc != receivedLunaBattery1Soc) {
                            m_lunaBattery1Soc = receivedLunaBattery1Soc;
                            emit lunaBattery1SocChanged(m_lunaBattery1Soc);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Luna 2000 Battery 1 state of charge\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Luna 2000 Battery 1 state of charge\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterPowerMeterActivePower: {
        // Update registers from Power meter active power
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Power meter active power\" register:" << 37113 << "size:" << 2;
        QModbusReply *reply = readPowerMeterActivePower();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Power meter active power\" register" << 37113 << "size:" << 2 << values;
                        qint32 receivedPowerMeterActivePower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                        if (m_powerMeterActivePower != receivedPowerMeterActivePower) {
                            m_powerMeterActivePower = receivedPowerMeterActivePower;
                            emit powerMeterActivePowerChanged(m_powerMeterActivePower);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Power meter active power\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Power meter active power\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterLunaBattery2Status: {
        // Update registers from Luna 2000 Battery 2 status
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Luna 2000 Battery 2 status\" register:" << 37741 << "size:" << 1;
        QModbusReply *reply = readLunaBattery2Status();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Luna 2000 Battery 2 status\" register" << 37741 << "size:" << 1 << values;
                        BatteryDeviceStatus receivedLunaBattery2Status = static_cast<BatteryDeviceStatus>(ModbusDataUtils::convertToUInt16(values));
                        qCDebug(dcHuaweiFusionSolar()) << "Battery 2 status" << receivedLunaBattery2Status;
                        if (receivedLunaBattery2Status == BatteryDeviceStatusOffline) {
                            m_battery2Available = false;
                        } else {
                            m_battery2Available = true;
                        }
                        if (m_lunaBattery2Status != receivedLunaBattery2Status) {
                            m_lunaBattery2Status = receivedLunaBattery2Status;
                            emit lunaBattery2StatusChanged(m_lunaBattery2Status);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Luna 2000 Battery 2 status\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Luna 2000 Battery 2 status\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterLunaBattery2Power: {
        // Update registers from Luna 2000 Battery 2 power
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Luna 2000 Battery 2 power\" register:" << 37743 << "size:" << 2;
        QModbusReply *reply = readLunaBattery2Power();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Luna 2000 Battery 2 power\" register" << 37743 << "size:" << 2 << values;
                        qint32 receivedLunaBattery2Power = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                        if (m_lunaBattery2Power != receivedLunaBattery2Power) {
                            m_lunaBattery2Power = receivedLunaBattery2Power;
                            emit lunaBattery2PowerChanged(m_lunaBattery2Power);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Luna 2000 Battery 2 power\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Luna 2000 Battery 2 power\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    case HuaweiModbusTcpConnection::RegisterLunaBattery2Soc: {
        // Update registers from Luna 2000 Battery 2 state of charge
        qCDebug(dcHuaweiFusionSolar()) << "--> Read \"Luna 2000 Battery 2 state of charge\" register:" << 37738 << "size:" << 1;
        QModbusReply *reply = readLunaBattery2Soc();
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcHuaweiFusionSolar()) << "<-- Response from \"Luna 2000 Battery 2 state of charge\" register" << 37738 << "size:" << 1 << values;
                        float receivedLunaBattery2Soc = ModbusDataUtils::convertToUInt16(values) * 1.0 * pow(10, -1);
                        if (m_lunaBattery2Soc != receivedLunaBattery2Soc) {
                            m_lunaBattery2Soc = receivedLunaBattery2Soc;
                            emit lunaBattery2SocChanged(m_lunaBattery2Soc);
                        }
                    }
                    finishRequest();
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"Luna 2000 Battery 2 state of charge\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
                finishRequest();
            }
        } else {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"Luna 2000 Battery 2 state of charge\" registers from" << hostAddress().toString() << errorString();
            finishRequest();
        }
        break;
    }
    }
}

void HuaweiFusionSolar::finishRequest()
{
    m_currentRegisterRequest = -1;
    QTimer::singleShot(1000, this, &HuaweiFusionSolar::readNextRegister);
}