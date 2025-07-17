/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2021, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
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

#include "integrationpluginjanitza.h"
#include "plugininfo.h"
#include "discoverytcp.h"

IntegrationPluginJanitza::IntegrationPluginJanitza()
{
}

void IntegrationPluginJanitza::init()
{
}

void IntegrationPluginJanitza::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcJanitza()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }
    DiscoveryTcp *discovery = new DiscoveryTcp(hardwareManager()->networkDeviceDiscovery(), info);
    connect(discovery, &DiscoveryTcp::discoveryFinished, info, [=](){
        foreach (const DiscoveryTcp::Result &result, discovery->discoveryResults()) {
            QString name = supportedThings().findById(info->thingClassId()).displayName();
            ThingDescriptor descriptor(umg604ThingClassId, name);
            qCInfo(dcJanitza()) << "Discovered:" << descriptor.title() << descriptor.description();
            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(umg604ThingClassId, result.networkDeviceInfo.macAddress());
            if (existingThings.count() >= 1) {
                qCDebug(dcJanitza()) << "This Janitza energy meter already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }
            QString serialNumberString = QString::number(result.serialNumber);
            ParamList params;
            params << Param(umg604ThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
            params << Param(umg604ThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            params << Param(umg604ThingSerialNumberParamTypeId, serialNumberString);
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }
        info->finish(Thing::ThingErrorNoError);
    });
    discovery->startDiscovery();
}

void IntegrationPluginJanitza::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcJanitza()) << "Setup thing" << thing << thing->params();

    // Handle reconfigure
    if (m_umg604TcpConnections.contains(thing)) {
        qCDebug(dcJanitza()) << "Already have a Janitza connection for this thing. Cleaning up old connection and initializing new one...";
        delete m_umg604TcpConnections.take(thing);
    } else {
        qCDebug(dcJanitza()) << "Setting up a new device:" << thing->params();
    }
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
    MacAddress macAddress = MacAddress(thing->paramValue(umg604ThingMacAddressParamTypeId).toString());
    if (macAddress.isNull()) {
        qCWarning(dcJanitza()) << "Failed to set up Janitza meter because the MAC address is not valid:" << thing->paramValue(umg604ThingMacAddressParamTypeId).toString() << macAddress.toString();
        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
        return;
    }

    // Create the monitor
    NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
    m_monitors.insert(thing, monitor);
    connect(info, &ThingSetupInfo::aborted, monitor, [=](){
        // Clean up in case the setup gets aborted
        if (m_monitors.contains(thing)) {
            qCDebug(dcJanitza()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    qCInfo(dcJanitza()) << "Setting up Janitza";
    umg604ModbusTcpConnection *connection = new umg604ModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, this);

    connect(info, &ThingSetupInfo::aborted, connection, &umg604ModbusTcpConnection::deleteLater);

    // Initialization has finished
    connect(connection, &umg604ModbusTcpConnection::initializationFinished, thing, [info, this, connection, thing](bool success) {
        thing->setStateValue(umg604ConnectedStateTypeId, success);
        if (success) {
             // Set basic info about device
             qCDebug(dcJanitza()) << "Janitza energy meter initialized.";
             QString serialNumberRead = QString::number(connection->serialNumber());
             QString serialNumberConfig = thing->paramValue(umg604ThingSerialNumberParamTypeId).toString();
             int stringsNotEqual = QString::compare(serialNumberRead, serialNumberConfig, Qt::CaseInsensitive);  // if strings are equal, stringsNotEqual should be 0.
             if (stringsNotEqual) {
                 // The umg604 found is a different one than configured. We assume the umg604 was replaced, and the new device should use this config.
                 // Step 1: update the serial number.
                 qCDebug(dcJanitza()) << "The serial number of this device is" << serialNumberRead << ". It does not match the serial number in the config, which is"
                                      << serialNumberConfig << ". Updating config with new serial number.";
                 thing->setParamValue(umg604ThingSerialNumberParamTypeId, serialNumberRead);
             }
             qCDebug(dcJanitza()) << "Setting Firmware state.";
             thing->setStateValue(umg604FirmwareVersionStateTypeId, connection->firmwareVersion());

             connection->update();
        } else {
            qCDebug(dcJanitza()) << "Janitza energy meter initialization failed.";
            // Try to reconnect to device
            connection->reconnectDevice();
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
        qCDebug(dcJanitza()) << "Network device monitor reachable changed for" << thing->name() << reachable;
        if (!thing->setupComplete())
            return;

        if (reachable && !thing->stateValue("connected").toBool()) {
            connection->modbusTcpMaster()->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else if (!reachable) {
            // Note: Auto reconnect is disabled explicitly and
            // the device will be connected once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &umg604ModbusTcpConnection::reachableChanged, thing, [this, thing, connection](bool reachable){
        qCInfo(dcJanitza()) << "Reachable changed to" << reachable << "for" << thing;
        if (reachable) {
            // Connected true will be set after successfull init
            connection->initialize();
        } else {
            thing->setStateValue("connected", false);
            thing->setStateValue(umg604CurrentPowerStateTypeId, 0);
            thing->setStateValue(umg604CurrentPhaseAStateTypeId, 0);
            thing->setStateValue(umg604CurrentPhaseBStateTypeId, 0);
            thing->setStateValue(umg604CurrentPhaseCStateTypeId, 0);
            thing->setStateValue(umg604VoltagePhaseAStateTypeId, 0);
            thing->setStateValue(umg604VoltagePhaseBStateTypeId, 0);
            thing->setStateValue(umg604VoltagePhaseCStateTypeId, 0);
            thing->setStateValue(umg604FrequencyStateTypeId, 0);
        }
    });

    connect(connection, &umg604ModbusTcpConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
        thing->setStateValue(umg604CurrentPhaseAStateTypeId, currentPhaseA);
    });
    connect(connection, &umg604ModbusTcpConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
        thing->setStateValue(umg604CurrentPhaseBStateTypeId, currentPhaseB);
    });
    connect(connection, &umg604ModbusTcpConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
        thing->setStateValue(umg604CurrentPhaseCStateTypeId, currentPhaseC);
    });
    connect(connection, &umg604ModbusTcpConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
        thing->setStateValue(umg604VoltagePhaseAStateTypeId, voltagePhaseA);
    });
    connect(connection, &umg604ModbusTcpConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
        thing->setStateValue(umg604VoltagePhaseBStateTypeId, voltagePhaseB);
    });
    connect(connection, &umg604ModbusTcpConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
        thing->setStateValue(umg604VoltagePhaseCStateTypeId, voltagePhaseC);
    });
    connect(connection, &umg604ModbusTcpConnection::totalCurrentPowerChanged, this, [=](float currentPower){
        thing->setStateValue(umg604CurrentPowerStateTypeId, currentPower);
    });
    connect(connection, &umg604ModbusTcpConnection::frequencyChanged, this, [=](float frequency){
        thing->setStateValue(umg604FrequencyStateTypeId, frequency);
    });
    connect(connection, &umg604ModbusTcpConnection::totalEnergyConsumedChanged, this, [=](float consumedEnergy){
        thing->setStateValue(umg604TotalEnergyConsumedStateTypeId, consumedEnergy);
    });
    connect(connection, &umg604ModbusTcpConnection::totalEnergyProducedChanged, this, [=](float producedEnergy){
        thing->setStateValue(umg604TotalEnergyProducedStateTypeId, producedEnergy);
    });

    m_umg604TcpConnections.insert(thing, connection);

    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);

}

void IntegrationPluginJanitza::postSetupThing(Thing *thing)
{
    qCDebug(dcJanitza()) << "Post setup thing" << thing->name();
    if (!m_refreshTimer) {
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);

        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach(auto thing, myThings().filterByThingClassId(umg604ThingClassId)) {
                auto monitor = m_monitors.value(thing);
                if (!monitor->reachable()) {
                    continue;
                }

                auto connection = m_umg604TcpConnections.value(thing);
                if (connection->reachable()) {
                    qCDebug(dcJanitza()) << "Updating connection";
                    connection->update();
                } else {
                    qCDebug(dcJanitza()) << "Device not reachable. Probably a TCP connection error. Reconnecting TCP socket";
                    connection->reconnectDevice();
                }
            }
        });

        qCDebug(dcJanitza()) << "Starting refresh timer...";
        m_refreshTimer->start();
    }
}

void IntegrationPluginJanitza::thingRemoved(Thing *thing)
{
    qCDebug(dcJanitza()) << "Thing removed" << thing->name();

    if (m_umg604TcpConnections.contains(thing)) {
        auto connection = m_umg604TcpConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcJanitza()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}
