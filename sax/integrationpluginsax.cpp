/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Felix Stoecker <f.stoecker@consolinno.de>           *
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

#include "integrationpluginsax.h"
#include "plugininfo.h"

#include "saxmodbustcpconnection.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>
#include <plugintimer.h>

IntegrationPluginSax::IntegrationPluginSax()
{

}
void IntegrationPluginSax::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == saxStorageThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSax()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }
        ThingClass thingClass = supportedThings().findById(info->thingClassId()); // TODO can this be done easier?
        qCDebug(dcSax()) << "Starting network discovery...";
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=](){
            qCDebug(dcSax()) << "Discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "devices";
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
                qCDebug(dcSax()) << networkDeviceInfo;

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
                params << Param(saxStorageThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcSax()) << "Found already existing" << thingClass.name() << "battery:" << existingThing->name() << networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcSax()) << "Found new" << thingClass.name() << "battery";
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });

    }
}

void IntegrationPluginSax::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSax()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == saxStorageThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSax()) << "Already have a Sax connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_tcpConnections.take(thing);
        } else {
            qCDebug(dcSax()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));


        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(saxStorageThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcSax()) << "Failed to set up Sax Battery because the MAC address is not valid:" << thing->paramValue(saxStorageThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
        });

        qint16 port = thing->paramValue(saxStorageThingPortParamTypeId).toUInt();
        qint16 slaveId = thing->paramValue(saxStorageThingSlaveIdParamTypeId).toUInt();


        SaxModbusTcpConnection *connection = new SaxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        connect(info, &ThingSetupInfo::aborted, connection, &SaxModbusTcpConnection::deleteLater);

        connect(connection, &SaxModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable){
            qCDebug(dcSax()) << "Reachable state changed" << reachable;
            if (reachable) {
                connection->initialize();
            } else {
                thing->setStateValue(saxStorageConnectedStateTypeId, false);
            }
        });

        connect(connection, &SaxModbusTcpConnection::initializationFinished, info, [this, thing, connection, monitor, info](bool success){
            if (success) {
                qCDebug(dcSax()) << "Sax initialized.";
                m_tcpConnections.insert(thing, connection);
                m_monitors.insert(thing, monitor);
                info->finish(Thing::ThingErrorNoError);
            } else {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
                connection->deleteLater();
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the battery."));
            }
        });

        connect(connection, &SaxModbusTcpConnection::initializationFinished, thing, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue(saxStorageConnectedStateTypeId, true);
                connection->update();
            }
        });
        // Read Battery capacity directly via modbus
        double capacityDouble = connection->capacity()/ 1000.0;
        qCDebug(dcSax()) << "Battery capacity" << capacityDouble << "kWh";
        thing->setStateValue(saxStorageCapacityStateTypeId, capacityDouble);



        // Handle property changed signals
        /*current Power battery*/
        connect(connection, &SaxModbusTcpConnection::powerBatteryChanged, thing, [thing](qint16 currentPower){
            qCDebug(dcSax()) << "Battery power changed" << -currentPower << "W";
            thing->setStateValue(saxStorageCurrentPowerStateTypeId, -double(currentPower));
            if (currentPower < 0) {
                thing->setStateValue(saxStorageChargingStateStateTypeId, "discharging");
            } else if (currentPower > 0) {
                thing->setStateValue(saxStorageChargingStateStateTypeId, "charging");
            } else {
                thing->setStateValue(saxStorageChargingStateStateTypeId, "idle");
            }
        });

        /*battery SoC*/
        connect(connection, &SaxModbusTcpConnection::socBatteryChanged, thing, [thing](quint16 soc){
            qCDebug(dcSax()) << "Battery SoC changed" << soc << "%";
            if(soc < 20){
                thing->setStateValue(saxStorageBatteryCriticalStateTypeId, true);
            } else {
                thing->setStateValue(saxStorageBatteryCriticalStateTypeId, false);
            }
            thing->setStateValue(saxStorageSoCStateTypeId, soc);
        });

        /*battery current [A]*/
        connect(connection, &SaxModbusTcpConnection::currentBatteryChanged, thing, [thing](quint16 current){
            qCDebug(dcSax()) << "Battery current changed" << current << "A",
            thing->setStateValue(saxStorageCurrentStateTypeId, current);
        });

        connection->connectDevice();

        return;
    }
}

void IntegrationPluginSax::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == saxStorageThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcSax()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SaxModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginSax::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        m_tcpConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
