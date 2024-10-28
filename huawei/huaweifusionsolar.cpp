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
    HuaweiFusionModbusTcpConnection(hostAddress, port, slaveId, parent),
    m_slaveId(slaveId)
{
    // Note: sometimes right after the discovery / setup the check fails the first time due to server busy error,
    // this is a very slow or busy device since it returns quiet often that error. Don't faile with the first busy error...
    setCheckReachableRetries(3);

    connect(this, &HuaweiFusionModbusTcpConnection::connectionStateChanged, this, [=](bool connected){
        if (!connected) {
            m_registersQueue.clear();
        }
    });

    connect(this, &HuaweiFusionModbusTcpConnection::initializationFinished, this, [=](bool success) {
        if (success) {
            qCDebug(dcHuawei()) << "Huawei init finished successfully:" << model() << serialNumber() << productNumber();
        }
    });
}

bool HuaweiFusionSolar::update()
{
    if (!connected())
        return false;

    // Make sure there is not an update still running
    if (!m_registersQueue.isEmpty())
        return true;

    // Add the requests to queue.

    // initialize
    if (model().isEmpty()) {
        m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterModel);
    }

    // inverterStuff
    m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterInverterInputPower);

    // battery1
    if (m_battery1Available) {
        m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterLunaBattery1Status);
    } else {
        // Reading the battery registers tells us if the battery is available or not. If the battery is not available, check the registers
        // less often. But we do need to check, because this is how we detect if the battery is available.
        if (m_battery1timer >= MAX_BATTERY_TIMER) {
            m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterLunaBattery1Status);
            m_battery1timer = 0;
        } else {
            m_battery1timer++;
        }
    }

    // meterStuff
    m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterMeterGridVoltageAphase);

    // battery2
    if (m_battery2Available) {
        m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterLunaBattery2Soc);
    } else {
        // Reading the battery registers tells us if the battery is available or not. If the battery is not available, check the registers
        // less often. But we do need to check, because this is how we detect if the battery is available.
        if (m_battery2timer >= MAX_BATTERY_TIMER) {
            m_registersQueue.enqueue(HuaweiFusionModbusTcpConnection::RegisterLunaBattery2Soc);
            m_battery2timer = 0;
        } else {
            m_battery2timer++;
        }
    }

    // Note: since huawei can only process one request at the time, we need to queue the requests and have some time between requests...

    m_currentRegisterRequest = -1;
    readNextRegister();
    return true;
}

quint16 HuaweiFusionSolar::slaveId() const
{
    return m_slaveId;
}

void HuaweiFusionSolar::readNextRegister()
{
    // Check if currently a reply is pending
    if (m_currentRegisterRequest >= 0)
        return;

    // Check if there is still a register to read
    if (m_registersQueue.isEmpty())
        return;

    // Registers are removed from the head of the queue if the request was sucessful. If a request failed, try the same
    // register again until the request is sucessful or MAX_REQUEST_RETRY_COUNT is reached.
    m_currentRegisterRequest = m_registersQueue.head();

    switch (m_currentRegisterRequest) {
    case HuaweiFusionModbusTcpConnection::RegisterModel: {
        // Update register block "identifyer"
        qCDebug(dcHuaweiFusionSolar()) << "--> Read block \"identifyer\" registers from:" << 30000 << "size:" << 35;
        QModbusReply *reply = readBlockIdentifyer();
        if (!reply) {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading block \"identifyer\" registers";
            finishRequestRetryIs(false);
            return;
        }

        if (reply->isFinished()) {
            reply->deleteLater(); // Broadcast reply returns immediatly
            finishRequestRetryIs(false);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, this, [this, reply](){
            handleModbusError(reply->error());
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit unit = reply->result();
                const QVector<quint16> blockValues = unit.values();
                qCDebug(dcHuaweiFusionSolar()) << "<-- Response from reading block \"identifyer\" register" << 30000 << "size:" << 35 << blockValues;
                if (!valuesAreVaild(unit.values(), 35)) {
                    qCWarning(dcHuaweiFusionSolar()) << "<-- Received invalid values. Requested" << 35 << "but received" << unit.values();
                    finishRequestRetryIs(true);
                } else {
                    processModelRegisterValues(blockValues.mid(0, 15));
                    processSerialNumberRegisterValues(blockValues.mid(15, 10));
                    processProductNumberRegisterValues(blockValues.mid(25, 10));

                    emit initializationFinished(true);
                    finishRequestRetryIs(false);
                }
            } else {
                finishRequestRetryIs(true);
            }
        });

        connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusResponse response = reply->rawResult();
                if (response.isException()) {
                    qCDebug(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"identifyer\" registers from" << hostAddress().toString() << exceptionToString(response.exceptionCode());
                }
            } else {
                qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"identifyer\" registers from" << hostAddress().toString() << error << reply->errorString();
            }
        });

        break;
    }
    case HuaweiFusionModbusTcpConnection::RegisterInverterInputPower: {
        // Update register block "inverterStuff"
        qCDebug(dcHuaweiFusionSolar()) << "--> Read block \"inverterStuff\" registers from:" << 32064 << "size:" << 44;
        QModbusReply *reply = readBlockInverterStuff();
        if (!reply) {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading block \"inverterStuff\" registers";
            finishRequestRetryIs(false);
            return;
        }

        if (reply->isFinished()) {
            reply->deleteLater(); // Broadcast reply returns immediatly
            finishRequestRetryIs(false);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, this, [this, reply](){
            handleModbusError(reply->error());
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit unit = reply->result();
                const QVector<quint16> blockValues = unit.values();
                qCDebug(dcHuaweiFusionSolar()) << "<-- Response from reading block \"inverterStuff\" register" << 32064 << "size:" << 44 << blockValues;
                if (!valuesAreVaild(unit.values(), 44)) {
                    qCWarning(dcHuaweiFusionSolar()) << "<-- Received invalid values. Requested" << 44 << "but received" << unit.values();
                    finishRequestRetryIs(true);
                } else {
                    processInverterInputPowerRegisterValues(blockValues.mid(0, 2));
                    //processFillerRegister1RegisterValues(blockValues.mid(2, 14));
                    processInverterActivePowerRegisterValues(blockValues.mid(16, 2));
                    //processFillerRegister2RegisterValues(blockValues.mid(18, 7));
                    processInverterDeviceStatusRegisterValues(blockValues.mid(25, 1));
                    //processFillerRegister3RegisterValues(blockValues.mid(26, 16));
                    processInverterEnergyProducedRegisterValues(blockValues.mid(42, 2));

                    emit inverterValuesUpdated();
                    finishRequestRetryIs(false);
                }
            } else {
                finishRequestRetryIs(true);
            }
        });

        connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusResponse response = reply->rawResult();
                if (response.isException()) {
                    qCDebug(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"inverterStuff\" registers from" << hostAddress().toString() << exceptionToString(response.exceptionCode());
                }
            } else {
                qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"inverterStuff\" registers from" << hostAddress().toString() << error << reply->errorString();
            }
        });

        break;
    }
    case HuaweiFusionModbusTcpConnection::RegisterLunaBattery1Status: {
        // Update register block "battery1"
        qCDebug(dcHuaweiFusionSolar()) << "--> Read block \"battery1\" registers from:" << 37000 << "size:" << 5;
        QModbusReply *reply = readBlockBattery1();
        if (!reply) {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading block \"battery1\" registers";
            finishRequestRetryIs(false);
            return;
        }

        if (reply->isFinished()) {
            reply->deleteLater(); // Broadcast reply returns immediatly
            finishRequestRetryIs(false);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, this, [this, reply](){
            handleModbusError(reply->error());
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit unit = reply->result();
                const QVector<quint16> blockValues = unit.values();
                qCDebug(dcHuaweiFusionSolar()) << "<-- Response from reading block \"battery1\" register" << 37000 << "size:" << 5 << blockValues;
                if (!valuesAreVaild(unit.values(), 5)) {
                    qCWarning(dcHuaweiFusionSolar()) << "<-- Received invalid values. Requested" << 5 << "but received" << unit.values();
                    finishRequestRetryIs(true);
                } else {
                    processLunaBattery1StatusRegisterValues(blockValues.mid(0, 1));
                    switch (m_lunaBattery1Status) {
                    case HuaweiFusionSolar::BatteryDeviceStatusFault:
                    case HuaweiFusionSolar::BatteryDeviceStatusStandby:
                    case HuaweiFusionSolar::BatteryDeviceStatusOffline:
                    case HuaweiFusionSolar::BatteryDeviceStatusSleepMode:
                        m_battery1Available = false;
                        m_lunaBattery1Power = 0;
                        emit lunaBattery1PowerChanged(m_lunaBattery1Power);
                        break;
                    case HuaweiFusionSolar::BatteryDeviceStatusRunning:
                        m_battery1Available = true;
                        break;
                    }
                    if (m_battery1Available) {
                        processLunaBattery1PowerRegisterValues(blockValues.mid(1, 2));
                        //processFillerRegister4RegisterValues(blockValues.mid(3, 1));
                    }
                    processLunaBattery1SocRegisterValues(blockValues.mid(4, 1));

                    emit battery1ValuesUpdated();
                    finishRequestRetryIs(false);
                }
            } else {
                finishRequestRetryIs(true);
            }
        });

        connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusResponse response = reply->rawResult();
                if (response.isException()) {
                    qCDebug(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"battery1\" registers from" << hostAddress().toString() << exceptionToString(response.exceptionCode());
                }
            } else {
                qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"battery1\" registers from" << hostAddress().toString() << error << reply->errorString();
            }
        });

        break;
    }
    case HuaweiFusionModbusTcpConnection::RegisterMeterGridVoltageAphase: {
        // Update register block "meterStuff"
        qCDebug(dcHuaweiFusionSolar()) << "--> Read block \"meterStuff\" registers from:" << 37101 << "size:" << 37;
        QModbusReply *reply = readBlockMeterStuff();
        if (!reply) {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading \"meterStuff\" registers from" << hostAddress().toString() << errorString();
            finishRequestRetryIs(false);
            return;
        }

        if (reply->isFinished()) {
            reply->deleteLater(); // Broadcast reply returns immediatly
            finishRequestRetryIs(false);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, this, [this, reply](){
            handleModbusError(reply->error());
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit unit = reply->result();
                const QVector<quint16> blockValues = unit.values();
                qCDebug(dcHuaweiFusionSolar()) << "<-- Response from reading block \"meterStuff\" register" << 37101 << "size:" << 37 << blockValues;
                if (!valuesAreVaild(unit.values(), 37)) {
                    qCWarning(dcHuaweiFusionSolar()) << "<-- Received invalid values. Requested" << 37 << "but received" << unit.values();
                    finishRequestRetryIs(true);
                } else {
                    processMeterGridVoltageAphaseRegisterValues(blockValues.mid(0, 2));
                    processMeterGridVoltageBphaseRegisterValues(blockValues.mid(2, 2));
                    processMeterGridVoltageCphaseRegisterValues(blockValues.mid(4, 2));
                    processMeterGridCurrentAphaseRegisterValues(blockValues.mid(6, 2));
                    processMeterGridCurrentBphaseRegisterValues(blockValues.mid(8, 2));
                    processMeterGridCurrentCphaseRegisterValues(blockValues.mid(10, 2));
                    processPowerMeterActivePowerRegisterValues(blockValues.mid(12, 2));
                    processMeterReactivePowerRegisterValues(blockValues.mid(14, 2));
                    processMeterPowerFactorRegisterValues(blockValues.mid(16, 1));
                    processMeterGridFrequencyRegisterValues(blockValues.mid(17, 1));
                    processPowerMeterEnergyReturnedRegisterValues(blockValues.mid(18, 2));
                    processPowerMeterEnergyAquiredRegisterValues(blockValues.mid(20, 2));
                    //processFillerRegister5RegisterValues(blockValues.mid(22, 9));
                    processMeterAphaseActivePowerRegisterValues(blockValues.mid(31, 2));
                    processMeterBphaseActivePowerRegisterValues(blockValues.mid(33, 2));
                    processMeterCphaseActivePowerRegisterValues(blockValues.mid(35, 2));

                    emit meterValuesUpdated();
                    finishRequestRetryIs(false);
                }
            } else {
                finishRequestRetryIs(true);
            }
        });

        connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusResponse response = reply->rawResult();
                if (response.isException()) {
                    qCDebug(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"meterStuff\" registers from" << hostAddress().toString() << exceptionToString(response.exceptionCode());
                }
            } else {
                qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"meterStuff\" registers from" << hostAddress().toString() << error << reply->errorString();
            }
        });

        break;
    }
    case HuaweiFusionModbusTcpConnection::RegisterLunaBattery2Soc: {
    // Update register block "battery2"1"
        qCDebug(dcHuaweiFusionSolar()) << "--> Read block \"battery2\" registers from:" << 37738 << "size:" << 7;
        QModbusReply *reply = readBlockBattery2();
        if (!reply) {
            qCWarning(dcHuaweiFusionSolar()) << "Error occurred while reading block \"battery2\" registers";
            finishRequestRetryIs(false);
            return;
        }

        if (reply->isFinished()) {
            reply->deleteLater(); // Broadcast reply returns immediatly
            finishRequestRetryIs(false);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, this, [this, reply](){
            handleModbusError(reply->error());
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit unit = reply->result();
                const QVector<quint16> blockValues = unit.values();
                qCDebug(dcHuaweiFusionSolar()) << "<-- Response from reading block \"battery2\" register" << 37738 << "size:" << 7 << blockValues;
                if (!valuesAreVaild(unit.values(), 7)) {
                    qCWarning(dcHuaweiFusionSolar()) << "<-- Received invalid values. Requested" << 7 << "but received" << unit.values();
                    finishRequestRetryIs(true);
                } else {
                    processLunaBattery2SocRegisterValues(blockValues.mid(0, 1));
                    //processFillerRegister6RegisterValues(blockValues.mid(1, 2));
                    processLunaBattery2StatusRegisterValues(blockValues.mid(3, 1));
                    switch (m_lunaBattery2Status) {
                    case HuaweiFusionSolar::BatteryDeviceStatusFault:
                    case HuaweiFusionSolar::BatteryDeviceStatusStandby:
                    case HuaweiFusionSolar::BatteryDeviceStatusOffline:
                    case HuaweiFusionSolar::BatteryDeviceStatusSleepMode:
                        m_battery2Available = false;
                        m_lunaBattery2Power = 0;
                        emit lunaBattery2PowerChanged(m_lunaBattery2Power);
                        break;
                    case HuaweiFusionSolar::BatteryDeviceStatusRunning:
                        m_battery2Available = true;
                        break;
                    }
                    if (m_battery2Available) {
                        //processFillerRegister7RegisterValues(blockValues.mid(4, 1));
                        processLunaBattery2PowerRegisterValues(blockValues.mid(5, 2));
                    }

                    emit battery2ValuesUpdated();
                    finishRequestRetryIs(false);
                }
            } else {
                finishRequestRetryIs(true);
            }
        });

        connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
            if (reply->error() == QModbusDevice::ProtocolError) {
                QModbusResponse response = reply->rawResult();
                if (response.isException()) {
                    qCDebug(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"battery2\" registers from" << hostAddress().toString() << exceptionToString(response.exceptionCode());
                }
            } else {
                qCWarning(dcHuaweiFusionSolar()) << "Modbus reply error occurred while updating \"battery2\" registers from" << hostAddress().toString() << error << reply->errorString();
            }
        });

        break;
    }
    default:
        // If this ever triggers, there is an error in the code.
        qCWarning(dcHuaweiFusionSolar()) << "Code error detected - an unknown register was in the queue:" << m_currentRegisterRequest;
        finishRequestRetryIs(false);
    }
}

bool HuaweiFusionSolar::valuesAreVaild(const QVector<quint16> &values, int readSize)
{
    if (values.count() != readSize) {
        qCDebug(dcHuaweiFusionSolar()) << "Invalid values. The received values count does not match the requested" << readSize << "registers.";
        return false;
    }

    // According to the documentation from 2016:
    // 0x7FFF: invalid value of the floating point type returned by one register
    // 0xFFFF: invalid value of a type other than the floating point type returned by one register
    // 0xFFFFFFFF: invalid value returned by two registers

    if (values.count() == 2) {
        bool floatingPointValid = (values != QVector<quint16>() << 0x7fff << 0xffff);
        if (!floatingPointValid)
            qCDebug(dcHuaweiFusionSolar()) << "Invalid values. The received values match the invalid for floating pointer:" << values;

        bool otherTypesValid = (values != QVector<quint16>(2, 0xffff));
        if (!otherTypesValid)
            qCDebug(dcHuaweiFusionSolar()) << "Invalid values. The received values match the invalid registers values:" << values;

        return floatingPointValid && otherTypesValid;
    }

    if (values.count() == 1)
        return values.at(0) != 0x7fff && values.at(0) != 0xffff;

    return true;
}

void HuaweiFusionSolar::finishRequestRetryIs(bool retryRequest)
{
    if (retryRequest) {
        // Leave the register in the queue to try again.
        m_requestRetryCounter++;
        if (m_requestRetryCounter > MAX_REQUEST_RETRY_COUNT) {
            m_registersQueue.dequeue(); // No point trying again. Move on to next item.
            m_requestRetryCounter = 0;
        }
    } else {
        m_registersQueue.dequeue();
        m_requestRetryCounter = 0;
    }
    m_currentRegisterRequest = -1;
    QTimer::singleShot(1000, this, &HuaweiFusionSolar::readNextRegister);
}

QString HuaweiFusionSolar::exceptionToString(QModbusPdu::ExceptionCode exception)
{
    QString exceptionString;
    switch (exception) {
    case QModbusPdu::IllegalFunction:
        exceptionString = "Illegal function";
        break;
    case QModbusPdu::IllegalDataAddress:
        exceptionString = "Illegal data address";
        break;
    case QModbusPdu::IllegalDataValue:
        exceptionString = "Illegal data value";
        break;
    case QModbusPdu::ServerDeviceFailure:
        exceptionString = "Server device failure";
        break;
    case QModbusPdu::Acknowledge:
        exceptionString = "Acknowledge";
        break;
    case QModbusPdu::ServerDeviceBusy:
        exceptionString = "Server device busy";
        break;
    case QModbusPdu::NegativeAcknowledge:
        exceptionString = "Negative acknowledge";
        break;
    case QModbusPdu::MemoryParityError:
        exceptionString = "Memory parity error";
        break;
    case QModbusPdu::GatewayPathUnavailable:
        exceptionString = "Gateway path unavailable";
        break;
    case QModbusPdu::GatewayTargetDeviceFailedToRespond:
        exceptionString = "Gateway target device failed to respond";
        break;
    case QModbusPdu::ExtendedException:
        exceptionString = "Extended exception";
        break;
    }

    return exceptionString;
}
