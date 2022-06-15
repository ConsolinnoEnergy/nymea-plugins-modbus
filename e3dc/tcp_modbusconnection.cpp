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

QList<int> TCP_ModbusConnection::activeWallbox()
{
    return m_activeWallboxes;
}

float TCP_ModbusConnection::currentPowerInv() const
{
    return m_currentPowerInv;
}

int TCP_ModbusConnection::WallboxActive()
{
    return m_wallboxesActive;
}

void TCP_ModbusConnection::setWallboxActive(int count)
{
    m_wallboxesActive = count;
}

int TCP_ModbusConnection::WallboxInactive()
{
    return m_wallboxesInactive;
}

void TCP_ModbusConnection::setWallboxInactive(int count)
{
    m_wallboxesInactive = count;
}

int TCP_ModbusConnection::MaximumAmountofWallboxes()
{
    return m_maximumAmountofWallboxes;
}

void TCP_ModbusConnection::initialize()
{
    // No init registers defined. Nothing to be done and we are finished.
    emit initializationFinished();
}


// these are the function which send the ModbusTcp call to get the data from the device
// and save it in the corresponding object
void TCP_ModbusConnection::updateCurrentPowerInv(e3dcInverter* inverter)
{
    // Update registers from Inverter current Power
    qCDebug(dcTCP_ModbusConnection()) << "--> Read \"Inverter current Power\" register:" << 40068 << "size:" << 2;
    QModbusReply *reply = readCurrentPowerInv();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, inverter, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"Inverter current Power\" register" << 40068 << "size:" << 2 << values;
                    float receivedCurrentPower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (inverter->currentPower() != receivedCurrentPower) {
                        inverter->setCurrentPower(receivedCurrentPower);
                        emit inverter->currentPowerChanged(receivedCurrentPower);
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

void TCP_ModbusConnection::updateCurrentPowerBat(e3dcBattery* battery)
{
    // Update registers from Inverter current Power
    qCDebug(dcTCP_ModbusConnection()) << "--> Read \"Battery current Power\" register:" << 40070 << "size:" << 2;
    QModbusReply *reply = readCurrentPowerBat();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, battery, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"Battery current Power\" register" << 40070 << "size:" << 2 << values;
                    float receivedCurrentPower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (battery->currentPower() != receivedCurrentPower) {
                        battery->setCurrentPower(receivedCurrentPower);
                        emit battery->currentPowerChanged(receivedCurrentPower);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcTCP_ModbusConnection()) << "Modbus reply error occurred while updating \"Battery current Power\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcTCP_ModbusConnection()) << "Error occurred while reading \"Battery current Power\" registers from" << hostAddress().toString() << errorString();
    }
}

void TCP_ModbusConnection::updateNetworkPointPower(e3dcInverter* inverter)
{
    // Update registers from Inverter current Power
    qCDebug(dcTCP_ModbusConnection()) << "--> Read \"NetworkPoint current Power\" register:" << 40074 << "size:" << 2;
    QModbusReply *reply = readNetworkPointPower();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, inverter, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"NetworkPoint current Power\" register" << 40074 << "size:" << 2 << values;
                    float receivedNetworkPointPower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (inverter->neworkPointPower() != receivedNetworkPointPower) {
                        inverter->setNetworkPointPower(receivedNetworkPointPower);
                        emit inverter->networkPointPowerChanged(receivedNetworkPointPower);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcTCP_ModbusConnection()) << "Modbus reply error occurred while updating \"NetworkPoint current Power\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcTCP_ModbusConnection()) << "Error occurred while reading \"NetworkPoint current Power\" registers from" << hostAddress().toString() << errorString();
    }
}

void TCP_ModbusConnection::updateCurrentPowerWB(e3dcWallbox* wallbox)
{
    // Update registers from Inverter current Power
    qCDebug(dcTCP_ModbusConnection()) << "--> Read \"Wallbox current Power\" register:" << 40078 << "size:" << 2;
    QModbusReply *reply = readCurrentPowerWB();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, wallbox, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"Wallbox current Power\" register" << 40078 << "size:" << 2 << values;
                    float receivedCurrentPower = ModbusDataUtils::convertToInt32(values, ModbusDataUtils::ByteOrderBigEndian);
                    if (wallbox->currentPower() != receivedCurrentPower) {
                        wallbox->setCurrentPower(receivedCurrentPower);
                        emit wallbox->currentPowerChanged(receivedCurrentPower);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcTCP_ModbusConnection()) << "Modbus reply error occurred while updating \"Wallbox current Power\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcTCP_ModbusConnection()) << "Error occurred while reading \"Wallbox current Power\" registers from" << hostAddress().toString() << errorString();
    }
}

void TCP_ModbusConnection::updateSOC(e3dcBattery* battery)
{
    // Update registers from Inverter current Power
    qCDebug(dcTCP_ModbusConnection()) << "--> Read \"Battery SOC  \" register:" << 40083 << "size:" << 1;
    QModbusReply *reply = readSOC();
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, battery, reply](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> values = unit.values();
                    qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"Battery SOC\" register" << 40083 << "size:" << 1 << values;
                    float SOC = ModbusDataUtils::convertToUInt16(values);
                    if (battery->SOC() != SOC) {
                        battery->setSOC(SOC);
                        emit battery->SOCChanged(SOC);
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                qCWarning(dcTCP_ModbusConnection()) << "Modbus reply error occurred while updating \"Battery SOC\" registers from" << hostAddress().toString() << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
        } else {
            delete reply; // Broadcast reply returns immediatly
        }
    } else {
        qCWarning(dcTCP_ModbusConnection()) << "Error occurred while reading \"Battery SOC\" registers from" << hostAddress().toString() << errorString();
    }
}





void TCP_ModbusConnection::updateInverter(e3dcInverter* inverter){
    // update all the attributes of the inverter
    updateCurrentPowerInv(inverter);
    updateNetworkPointPower(inverter);
}

void TCP_ModbusConnection::updateWallbox(e3dcWallbox* wallbox){
    // update all the attributes of the wallbox
    updateCurrentPowerWB(wallbox);
}

void TCP_ModbusConnection::updateBattery(e3dcBattery* battery){
    // update all the attributes of the battery
    updateCurrentPowerBat(battery);
    updateSOC(battery);
}

QModbusReply *TCP_ModbusConnection::readCurrentPowerInv()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 40068, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *TCP_ModbusConnection::readCurrentPowerBat()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 40070, 2);
    return sendReadRequest(request, m_slaveId);
}


QModbusReply *TCP_ModbusConnection::readNetworkPointPower()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 40074, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *TCP_ModbusConnection::readCurrentPowerWB()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 40078, 2);
    return sendReadRequest(request, m_slaveId);
}

QModbusReply *TCP_ModbusConnection::readSOC()
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, 40083, 1);
    return sendReadRequest(request, m_slaveId);
}


QModbusReply *TCP_ModbusConnection::readWallboxControl(int Register)
{
    QModbusDataUnit request = QModbusDataUnit(QModbusDataUnit::RegisterType::HoldingRegisters, Register, 1);
    return sendReadRequest(request, m_slaveId);
}

TCP_ModbusConnection::Error TCP_ModbusConnection::CheckWallboxes()
{
    // Maximum of 9 wallboxes on different register
    for(int i = 0; i < 8; i++ ){
        // we check the value on register 40088+i
        QModbusReply *reply = readWallboxControl(40088+i);
        if (reply) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, this, [this, i, reply](){
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit unit = reply->result();
                        const QVector<quint16> values = unit.values();
                        qCDebug(dcTCP_ModbusConnection()) << "<-- Response from \"Wallboxes  existence\" register" << 40088+i << "size:" << 1 << values;
                        quint16 receivedWallboxData = ModbusDataUtils::convertToUInt16(values);

                        // register 40088 - 40095 hold Wallbox CTRl data, every of these hold a Bitmask
                        // for the documentation on the bitmask check the e3dc manual @ https://community.symcon.de/uploads/short-url/z6Yc7LiO6m9lJt8r5Aif539GbHI.pdf  page 12+
                        // check for the 0 bit if the Wallbox exists.
                        if ((receivedWallboxData & TCP_ModbusConnection::ExistenceMask) == TCP_ModbusConnection::ExistenceMask)
                        {
                            // ExistenceMask is just 1 or 0000000000000001 if we and it only the last bit should stay if the bit is set (so the value is 1 or 0000000000000001) or 0 (or 000000000000000) if it isnt
                            qCDebug(dcTCP_ModbusConnection()) << " Wallbox at " << 40088+i <<   "exists";
                            // we save the wallboxes in a list. We only need to see if ONE wallbox is active or not since the information we get is cumulative
                            if (m_activeWallboxes.contains(40088+i)){
                                m_activeWallboxes.append(40088+i);
                                emit Countwallbox(true);
                            }


                        }else{

                            qCDebug(dcTCP_ModbusConnection()) << " Wallbox at " << 40088+i <<   "does not exist";
                            emit Countwallbox(false);
                            // remove wallboxes if they existed earlier.
                            if (m_activeWallboxes.contains(40088+i)){
                                qCDebug(dcTCP_ModbusConnection()) << " Wallbox at " << 40088+i <<   " was removed";
                                m_activeWallboxes.removeAt(m_activeWallboxes.indexOf(40088+i));
                            }

                        }


                    }
                });

                connect(reply, &QModbusReply::errorOccurred, this, [this, reply] (QModbusDevice::Error error){
                    qCWarning(dcTCP_ModbusConnection()) << "Modbus reply error occurred while updating \"Wallboxes  existence\" registers from" << hostAddress().toString() << error << reply->errorString();
                    emit reply->finished(); // To make sure it will be deleted
                });
            } else {
                delete reply; // Broadcast reply returns immediatly
            }
        } else {
            qCWarning(dcTCP_ModbusConnection()) << "Error occurred while reading \"Wallboxes  existence\" registers from" << hostAddress().toString() << errorString();
            return ConnectionWBError;
        }
    }
    return ConnectionNoError;

}


void TCP_ModbusConnection::verifyInitFinished()
{
    if (m_pendingInitReplies.isEmpty()) {
        qCDebug(dcTCP_ModbusConnection()) << "Initialization finished of TCP_ModbusConnection" << hostAddress().toString();
        emit initializationFinished();
    }
}

void TCP_ModbusConnection::startDiscovery()
{

    // check if Wallboxes exist
    // We always assume that an Inverter and a battery exist
    TCP_ModbusConnection::Error reply = CheckWallboxes();
    if (reply != TCP_ModbusConnection::ConnectionNoError){
        // if the reply sent an error give a warning and end it
        qCWarning(dcTCP_ModbusConnection()) << "There was an error with one of the Devices";
        emit discoveryFinished(false);
        return;
    }

    return;
}

QDebug operator<<(QDebug debug, TCP_ModbusConnection *tCP_ModbusConnection)
{
    debug.nospace().noquote() << "TCP_ModbusConnection(" << tCP_ModbusConnection->hostAddress().toString() << ":" << tCP_ModbusConnection->port() << ")" << "\n";
//    debug.nospace().noquote() << "    - Inverter current Power:" << tCP_ModbusConnection->currentPower() << " [kW]" << "\n";
    return debug.quote().space();
}

