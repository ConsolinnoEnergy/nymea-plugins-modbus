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

#include "integrationpluginsungrow.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

IntegrationPluginSungrow::IntegrationPluginSungrow()
{

}

void IntegrationPluginSungrow::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcSungrow()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(sungrowInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcSungrow()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(sungrowInverterRTUConnectedStateTypeId, false);
                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginSungrow::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == sungrowInverterTCPThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSungrow()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        qCDebug(dcSungrow()) << "Starting network discovery...";
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=](){
            qCDebug(dcSungrow()) << "Discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "devices";
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
                qCDebug(dcSungrow()) << networkDeviceInfo;

                QString title;
                if (networkDeviceInfo.hostName().isEmpty()) {
                    title = networkDeviceInfo.address().toString();
                } else {
                    title = networkDeviceInfo.hostName() + " (" + networkDeviceInfo.address().toString() + ")";
                }

                QString description;
                if (networkDeviceInfo.macAddressManufacturer().isEmpty()) {
                    description = networkDeviceInfo.macAddress();
                } else {
                    description = networkDeviceInfo.macAddress() + " (" + networkDeviceInfo.macAddressManufacturer() + ")";
                }

                ThingDescriptor descriptor(info->thingClassId(), title, description);
                ParamList params;
                params << Param(sungrowThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
                params << Param(sungrowThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcSungrow()) << "Found already existing" << thingClass.name() << "inverter:" << existingThing->name() << networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcSungrow()) << "Found new" << thingClass.name() << "inverter";
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });

    } else if (info->thingClassId() == sungrowInverterRTUThingClassId) {
        qCDebug(dcSungrow()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(sungrowInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcSungrow()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "Sungrow Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(sungrowInverterRTUThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(sungrowInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginSungrow::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSungrow()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == sungrowInverterTCPThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSungrow()) << "Already have a Sungrow connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_tcpConnections.take(thing);
        } else {
            qCDebug(dcSungrow()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));


        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(sungrowInverterTCPThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcSungrow()) << "Failed to set up Sungrow inverter because the MAC address is not valid:" << thing->paramValue(sungrowInverterTCPThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
        });

        uint port = thing->paramValue(sungrowInverterTCPThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(sungrowInverterTCPThingSlaveIdParamTypeId).toUInt();

        SungrowModbusTcpConnection *connection = new SungrowModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        connect(info, &ThingSetupInfo::aborted, connection, &SungrowModbusTcpConnection::deleteLater);

        connect(connection, &SungrowModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable){
            qCDebug(dcSungrow()) << "Reachable state changed" << reachable;
            if (reachable) {
                connection->initialize();
            } else {
                thing->setStateValue(sungrowInverterTCPConnectedStateTypeId, false);
            }
        });

        connect(connection, &SungrowModbusTcpConnection::initializationFinished, info, [this, thing, connection, monitor, info](bool success){
            if (success) {
                qCDebug(dcSungrow()) << "Sungrow inverter initialized.";
                m_tcpConnections.insert(thing, connection);
                m_monitors.insert(thing, monitor);
                info->finish(Thing::ThingErrorNoError);
            } else {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
                connection->deleteLater();
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the inverter."));
            }
        });

        connect(connection, &SungrowModbusTcpConnection::initializationFinished, thing, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue(sungrowInverterTCPConnectedStateTypeId, true);
                connection->update();
            }
        });

        // Handle property changed signals
        connect(connection, &SungrowModbusTcpConnection::activePowerChanged, thing, [thing](quint32 activePower){
            qCDebug(dcSungrow()) << "Inverter power changed" << activePower << "W";
            thing->setStateValue(sungrowInverterTCPCurrentPowerStateTypeId, activePower);
        });

        connect(connection, &SungrowModbusTcpConnection::deviceTypeCodeChanged, thing, [thing](quint16 deviceTypeCode){
            qCDebug(dcSungrow()) << "Inverter device type code recieved" << deviceTypeCode;
            Q_UNUSED(thing)
        });

        connect(connection, &SungrowModbusTcpConnection::totalPowerYieldsChanged, thing, [thing](quint32 totalEnergyProduced){
            qCDebug(dcSungrow()) << "Inverter total energy produced changed" << totalEnergyProduced << "kWh";
            thing->setStateValue(sungrowInverterTCPTotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        connection->connectDevice();

        return;
    }

    if (thing->thingClassId() == sungrowInverterRTUThingClassId) {

        uint address = thing->paramValue(sungrowInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address == 0) {
            qCWarning(dcSungrow()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(sungrowInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcSungrow()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcSungrow()) << "Already have a Sungrow connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        SungrowModbusRtuConnection *connection = new SungrowModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcSungrow()) << "Modbus RTU resource connected" << thing << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcSungrow()) << "Modbus RTU resource disconnected" << thing << connection->modbusRtuMaster()->serialPort();
            }
        });

        // Handle property changed signals
        connect(connection, &SungrowModbusRtuConnection::activePowerChanged, thing, [thing](quint32 activePower){
            qCDebug(dcSungrow()) << "Inverter power changed" << activePower << "W";
            thing->setStateValue(sungrowInverterRTUCurrentPowerStateTypeId, activePower);
            thing->setStateValue(sungrowInverterRTUConnectedStateTypeId, true);
        });

        connect(connection, &SungrowModbusRtuConnection::deviceTypeCodeChanged, thing, [thing](quint16 deviceTypeCode){
            qCDebug(dcSungrow()) << "Inverter device type code recieved" << deviceTypeCode;
            Q_UNUSED(thing)
        });

        connect(connection, &SungrowModbusRtuConnection::totalPowerYieldsChanged, thing, [thing](quint32 totalEnergyProduced){
            qCDebug(dcSungrow()) << "Inverter total energy produced changed" << totalEnergyProduced << "kWh";
            thing->setStateValue(sungrowInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        // FIXME: make async and check if this is really a sungrow
        m_rtuConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginSungrow::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == sungrowInverterTCPThingClassId || thing->thingClassId() == sungrowInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcSungrow()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SungrowModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }

                foreach(SungrowModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginSungrow::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        m_tcpConnections.take(thing)->deleteLater();
    }

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
