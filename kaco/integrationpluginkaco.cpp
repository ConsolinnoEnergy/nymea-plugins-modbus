/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2023 Sebastian Ringer <s.ringer@consolinno.de>           *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation;                  *
 *  version 3 of the License.                                              *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationpluginkaco.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

#include <QtMath>

IntegrationPluginKaco::IntegrationPluginKaco()
{

}

void IntegrationPluginKaco::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcKaco()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(kacoInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcKaco()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(kacoInverterRTUConnectedStateTypeId, false);
                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginKaco::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == kacoInverterTCPThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcKaco()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }
        ThingClass thingClass = supportedThings().findById(info->thingClassId()); // TODO can this be done easier?
        qCDebug(dcKaco()) << "Starting network discovery...";
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=](){
            qCDebug(dcKaco()) << "Discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "devices";
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
                qCDebug(dcKaco()) << networkDeviceInfo;

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
                params << Param(kacoInverterTCPThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcKaco()) << "Found already existing" << thingClass.name() << "inverter:" << existingThing->name() << networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcKaco()) << "Found new" << thingClass.name() << "inverter";
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });

    } else if (info->thingClassId() == kacoInverterRTUThingClassId) {
        qCDebug(dcKaco()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(kacoInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress < 3) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 3 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcKaco()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "Kaco Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(kacoInverterRTUThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(kacoInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginKaco::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcKaco()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == kacoInverterTCPThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcKaco()) << "Already have a Kaco connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_tcpConnections.take(thing);
        } else {
            qCDebug(dcKaco()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_scalefactors.contains(thing))
            m_scalefactors.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(kacoInverterTCPThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcKaco()) << "Failed to set up Kaco inverter because the MAC address is not valid:" << thing->paramValue(kacoInverterTCPThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
        });

        uint port = thing->paramValue(kacoInverterTCPThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(kacoInverterTCPThingSlaveIdParamTypeId).toUInt();

        KacoModbusTcpConnection *connection = new KacoModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        connection->setTimeout(4500);
        connect(info, &ThingSetupInfo::aborted, connection, &KacoModbusTcpConnection::deleteLater);

        connect(connection, &KacoModbusTcpConnection::reachableChanged, this, [connection, thing](bool reachable){
            qCDebug(dcKaco()) << "Reachable state changed" << reachable;
            if (reachable) {
                connection->initialize();
            } else {
                thing->setStateValue(kacoInverterTCPConnectedStateTypeId, false);
            }
        });

        connect(connection, &KacoModbusTcpConnection::initializationFinished, info, [this, thing, connection, monitor, info](bool success){
            if (success) {
                qCDebug(dcKaco()) << "Kaco inverter initialized.";
                m_tcpConnections.insert(thing, connection);
                m_monitors.insert(thing, monitor);
                ScaleFactors scalefactors{};
                m_scalefactors.insert(thing, scalefactors);
                info->finish(Thing::ThingErrorNoError);
            } else {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
                connection->deleteLater();
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the inverter."));
            }
        });

        connect(connection, &KacoModbusTcpConnection::initializationFinished, this, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue(kacoInverterTCPConnectedStateTypeId, true);
                connection->update();
            }
        });

        // Handle property changed signals
        connect(connection, &KacoModbusTcpConnection::activePowerSfChanged, this, [this, thing](qint16 activePowerSf){
            qCDebug(dcKaco()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &KacoModbusTcpConnection::activePowerChanged, this, [this, thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * qPow(10, m_scalefactors.value(thing).powerSf);
            qCDebug(dcKaco()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacoInverterTCPCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &KacoModbusTcpConnection::totalEnergyProducedSfChanged, this, [this, thing](qint16 totalEnergyProducedSf){
            qCDebug(dcKaco()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &KacoModbusTcpConnection::totalEnergyProducedChanged, this, [this, thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = (totalEnergyProduced * qPow(10, m_scalefactors.value(thing).energySf)) / 1000.0;
            qCDebug(dcKaco()) << "Inverter total energy produced changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(kacoInverterTCPTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });

        connect(connection, &KacoModbusTcpConnection::operatingStateChanged, this, [this, thing](KacoModbusTcpConnection::OperatingState operatingState){
            qCDebug(dcKaco()) << "Inverter operating state recieved" << operatingState;
            setOperatingState(thing, operatingState);
        });

        connection->connectDevice();

        return;
    }

    if (thing->thingClassId() == kacoInverterRTUThingClassId) {

        uint address = thing->paramValue(kacoInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address < 3) {
            qCWarning(dcKaco()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 3 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(kacoInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcKaco()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcKaco()) << "Already have a Kaco connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (m_scalefactors.contains(thing))
            m_scalefactors.remove(thing);

        KacoModbusRtuConnection *connection = new KacoModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcKaco()) << "Modbus RTU resource connected" << thing << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcKaco()) << "Modbus RTU resource disconnected" << thing << connection->modbusRtuMaster()->serialPort();
            }
        });

        // Handle property changed signals
        connect(connection, &KacoModbusRtuConnection::activePowerSfChanged, this, [this, thing](qint16 activePowerSf){
            qCDebug(dcKaco()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &KacoModbusRtuConnection::activePowerChanged, this, [this, thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * qPow(10, m_scalefactors.value(thing).powerSf);
            qCDebug(dcKaco()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacoInverterRTUCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &KacoModbusRtuConnection::totalEnergyProducedSfChanged, this, [this, thing](qint16 totalEnergyProducedSf){
            qCDebug(dcKaco()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &KacoModbusRtuConnection::totalEnergyProducedChanged, this, [this, thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = (totalEnergyProduced * qPow(10, m_scalefactors.value(thing).energySf)) / 1000;
            qCDebug(dcKaco()) << "Inverter total energy produced changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(kacoInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });

        connect(connection, &KacoModbusRtuConnection::operatingStateChanged, this, [this, thing](KacoModbusRtuConnection::OperatingState operatingState){
            qCDebug(dcKaco()) << "Inverter operating state recieved" << operatingState;
            setOperatingState(thing, operatingState);
        });


        // FIXME: make async and check if this is really a kaco
        m_rtuConnections.insert(thing, connection);
        ScaleFactors scalefactors{};
        m_scalefactors.insert(thing, scalefactors);
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginKaco::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == kacoInverterTCPThingClassId || thing->thingClassId() == kacoInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcKaco()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(KacoModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }

                foreach(KacoModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginKaco::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        m_tcpConnections.take(thing)->deleteLater();
    }

    if (m_scalefactors.contains(thing))
        m_scalefactors.remove(thing);

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginKaco::setOperatingState(Thing *thing, KacoModbusTcpConnection::OperatingState state)
{
    switch (state) {
        case KacoModbusTcpConnection::OperatingStateOff:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "Off");
            break;
        case KacoModbusTcpConnection::OperatingStateSleeping:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "Sleeping");
            break;
        case KacoModbusTcpConnection::OperatingStateStarting:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "Starting");
            break;
        case KacoModbusTcpConnection::OperatingStateMppt:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "MPPT");
            break;
        case KacoModbusTcpConnection::OperatingStateThrottled:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "Throttled");
            break;
        case KacoModbusTcpConnection::OperatingStateShuttingDown:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "ShuttingDown");
            break;
        case KacoModbusTcpConnection::OperatingStateFault:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "Fault");
            break;
        case KacoModbusTcpConnection::OperatingStateStandby:
            thing->setStateValue(kacoInverterTCPOperatingStateStateTypeId, "Standby");
            break;
    }
}


void IntegrationPluginKaco::setOperatingState(Thing *thing, KacoModbusRtuConnection::OperatingState state)
{
    switch (state) {
        case KacoModbusRtuConnection::OperatingStateOff:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "Off");
            break;
        case KacoModbusRtuConnection::OperatingStateSleeping:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "Sleeping");
            break;
        case KacoModbusRtuConnection::OperatingStateStarting:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "Starting");
            break;
        case KacoModbusRtuConnection::OperatingStateMppt:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "MPPT");
            break;
        case KacoModbusRtuConnection::OperatingStateThrottled:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "Throttled");
            break;
        case KacoModbusRtuConnection::OperatingStateShuttingDown:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "ShuttingDown");
            break;
        case KacoModbusRtuConnection::OperatingStateFault:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "Fault");
            break;
        case KacoModbusRtuConnection::OperatingStateStandby:
            thing->setStateValue(kacoInverterRTUOperatingStateStateTypeId, "Standby");
            break;
    }
}
