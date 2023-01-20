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

#include "integrationpluginkaco.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

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
        connect(info, &ThingSetupInfo::aborted, connection, &KacoModbusTcpConnection::deleteLater);

        connect(connection, &KacoModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable){
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

        connect(connection, &KacoModbusTcpConnection::initializationFinished, thing, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue(kacoInverterTCPConnectedStateTypeId, true);
                connection->update();
            }
        });

        // Handle property changed signals
        connect(connection, &KacoModbusTcpConnection::activePowerSfChanged, thing, [thing](qint16 activePowerSf){
            qCDebug(dcKaco()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &KacoModbusTcpConnection::activePowerChanged, thing, [thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * m_scalefactors.value(thing).powerSf;
            qCDebug(dcKaco()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacoInverterTCPCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &KacoModbusTcpConnection::totalEnergyProducedSfChanged, thing, [thing](qint16 totalEnergyProducedSf){
            qCDebug(dcKaco()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &KacoModbusTcpConnection::totalPowerYieldsChanged, thing, [thing](quint32 totalEnergyProduced){
            quint32 totalEnergyProducedkWh = (totalEnergyProduced * m_scalefactors.value(thing).energySf) / 1000;
            qCDebug(dcKaco()) << "Inverter total energy produced changed" << totalEnergyProducedkWh << "kWh";
            thing->setStateValue(kacoInverterTCPTotalEnergyProducedStateTypeId, totalEnergyProducedkWh);
        });

        connect(connection, &KacoModbusTcpConnection::operatingStateChanged, thing, [thing](KacoModbusTcpConnection::OperatingState operatingState){
            qCDebug(dcKaco()) << "Inverter operating state recieved" << operatingState;
            Q_UNUSED(thing)
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
        connect(connection, &KacoModbusRtuConnection::activePowerSfChanged, thing, [thing](qint16 activePowerSf){
            qCDebug(dcKaco()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &KacoModbusRtuConnection::activePowerChanged, thing, [thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * m_scalefactors.value(thing).powerSf;
            qCDebug(dcKaco()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacoInverterRTUCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &KacoModbusRtuConnection::totalEnergyProducedSfChanged, thing, [thing](qint16 totalEnergyProducedSf){
            qCDebug(dcKaco()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &KacoModbusRtuConnection::totalPowerYieldsChanged, thing, [thing](quint32 totalEnergyProduced){
            quint32 totalEnergyProducedkWh = (totalEnergyProduced * m_scalefactors.value(thing).energySf) / 1000;
            qCDebug(dcKaco()) << "Inverter total energy produced changed" << totalEnergyProducedkWh << "kWh";
            thing->setStateValue(kacoInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProducedkWh);
        });

        connect(connection, &KacoModbusRtuConnection::operatingStateChanged, thing, [thing](KacoModbusRtuConnection::OperatingState operatingState){
            qCDebug(dcKaco()) << "Inverter operating state recieved" << operatingState;
            Q_UNUSED(thing)
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
