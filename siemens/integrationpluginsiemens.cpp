/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
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

#include "integrationpluginsiemens.h"
#include "plugininfo.h"
#include "discoverytcp.h"

IntegrationPluginSiemens::IntegrationPluginSiemens()
{
}

void IntegrationPluginSiemens::init()
{
}

void IntegrationPluginSiemens::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcSiemens()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }
    DiscoveryTcp *discovery = new DiscoveryTcp(hardwareManager()->networkDeviceDiscovery(), info);
    connect(discovery, &DiscoveryTcp::discoveryFinished, info, [=](){
        foreach (const DiscoveryTcp::Result &result, discovery->discoveryResults()) {
            QString name = supportedThings().findById(info->thingClassId()).displayName();
            ThingDescriptor descriptor(pac2200ThingClassId, name);
            qCInfo(dcSiemens()) << "Discovered:" << descriptor.title() << descriptor.description();
            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(pac2200ThingClassId, result.networkDeviceInfo.macAddress());
            if (existingThings.count() >= 1) {
                qCDebug(dcSiemens()) << "This SiemensPAC2200 energy meter already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }
            ParamList params;
            params << Param(pac2200ThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
            params << Param(pac2200ThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }
        info->finish(Thing::ThingErrorNoError);
    });
    discovery->startDiscovery();
}

void IntegrationPluginSiemens::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSiemens()) << "Setup thing" << thing << thing->params();

    // Handle reconfigure
    if (m_pac220TcpConnections.contains(thing)) {
        qCDebug(dcSiemens()) << "Already have a Siemens connection for this thing. Cleaning up old connection and initializing new one...";
        delete m_pac220TcpConnections.take(thing);
    } else {
        qCDebug(dcSiemens()) << "Setting up a new device:" << thing->params();
    }
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
    MacAddress macAddress = MacAddress(thing->paramValue(pac2200ThingMacAddressParamTypeId).toString());
    if (macAddress.isNull()) {
        qCWarning(dcSiemens()) << "Failed to set up Siemens meter because the MAC address is not valid:" << thing->paramValue(pac2200ThingMacAddressParamTypeId).toString() << macAddress.toString();
        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
        return;
    }

    // Create the monitor
    NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
    m_monitors.insert(thing, monitor);
    connect(info, &ThingSetupInfo::aborted, monitor, [=](){
        // Clean up in case the setup gets aborted
        if (m_monitors.contains(thing)) {
            qCDebug(dcSiemens()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    qCInfo(dcSiemens()) << "Setting up Siemens PAC2200";
    SiemensPAC2200ModbusTcpConnection *connection = new SiemensPAC2200ModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, this);

    connect(info, &ThingSetupInfo::aborted, connection, &SiemensPAC2200ModbusTcpConnection::deleteLater);

    // Initialization has finished
    connect(connection, &SiemensPAC2200ModbusTcpConnection::initializationFinished, thing, [info, this, connection, thing](bool success) {
        thing->setStateValue(pac2200ConnectedStateTypeId, success);
        if (success) {
             // Set basic info about device
             qCDebug(dcSiemens()) << "Siemens energy meter initialized.";
             connection->update();
        } else {
            qCDebug(dcSiemens()) << "Siemens energy meter initialization failed.";
            // Try to reconnect to device
            connection->reconnectDevice();
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
        qCDebug(dcSiemens()) << "Network device monitor reachable changed for" << thing->name() << reachable;
        if (!thing->setupComplete())
            return;

        if (reachable && !thing->stateValue("connected").toBool()) {
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else if (!reachable) {
            // Note: Auto reconnect is disabled explicitly and
            // the device will be connected once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &SiemensPAC2200ModbusTcpConnection::reachableChanged, thing, [this, thing, connection](bool reachable){
        qCInfo(dcSiemens()) << "Reachable changed to" << reachable << "for" << thing;
        if (reachable) {
            // Connected true will be set after successfull init
            connection->initialize();
        } else {
            thing->setStateValue("connected", false);
            thing->setStateValue(pac2200VoltagePhaseAStateTypeId, 0);
            thing->setStateValue(pac2200VoltagePhaseBStateTypeId, 0);
            thing->setStateValue(pac2200VoltagePhaseCStateTypeId, 0);
            thing->setStateValue(pac2200CurrentPhaseAStateTypeId, 0);
            thing->setStateValue(pac2200CurrentPhaseBStateTypeId, 0);
            thing->setStateValue(pac2200CurrentPhaseCStateTypeId, 0);
            thing->setStateValue(pac2200CurrentPowerStateTypeId, 0);
        }
    });

    connect(connection, &SiemensPAC2200ModbusTcpConnection::currentPhaseAChanged, this, [thing](float currentPhaseA){
        qCDebug(dcSiemens()) << "Curren phase A" << currentPhaseA;
        thing->setStateValue(pac2200CurrentPhaseAStateTypeId, currentPhaseA);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::currentPhaseBChanged, this, [thing](float currentPhaseB){
        qCDebug(dcSiemens()) << "Curren phase B" << currentPhaseB;
        thing->setStateValue(pac2200CurrentPhaseBStateTypeId, currentPhaseB);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::currentPhaseCChanged, this, [thing](float currentPhaseC){
        qCDebug(dcSiemens()) << "Curren phase C" << currentPhaseC;
        thing->setStateValue(pac2200CurrentPhaseCStateTypeId, currentPhaseC);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::voltagePhaseAChanged, this, [thing](float voltagePhaseA){
        qCDebug(dcSiemens()) << "Volage phase A" << voltagePhaseA;
        thing->setStateValue(pac2200VoltagePhaseAStateTypeId, voltagePhaseA);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::voltagePhaseBChanged, this, [thing](float voltagePhaseB){
        qCDebug(dcSiemens()) << "Volage phase B" << voltagePhaseB;
        thing->setStateValue(pac2200VoltagePhaseBStateTypeId, voltagePhaseB);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::voltagePhaseCChanged, this, [thing](float voltagePhaseC){
        qCDebug(dcSiemens()) << "Volage phase C" << voltagePhaseC;
        thing->setStateValue(pac2200VoltagePhaseCStateTypeId, voltagePhaseC);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::currentPowerChanged, this, [thing](float currentPower){
        qCDebug(dcSiemens()) << "Current power" << currentPower;
        thing->setStateValue(pac2200CurrentPowerStateTypeId, currentPower);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::frequencyChanged, this, [thing](float frequency){
        qCDebug(dcSiemens()) << "Frequency" << frequency;
        thing->setStateValue(pac2200FrequencyStateTypeId, frequency);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::totalEnergyConsumedChanged, this, [thing](float consumedEnergy){
        thing->setStateValue(pac2200TotalEnergyConsumedStateTypeId, consumedEnergy);
    });
    connect(connection, &SiemensPAC2200ModbusTcpConnection::totalEnergyProducedChanged, this, [thing](float producedEnergy){
        thing->setStateValue(pac2200TotalEnergyProducedStateTypeId, producedEnergy);
    });

    m_pac220TcpConnections.insert(thing, connection);

    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginSiemens::postSetupThing(Thing *thing)
{
    qCDebug(dcSiemens()) << "Post setup thing" << thing->name();
    if (!m_pluginTimer) {
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);

        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(auto thing, myThings().filterByThingClassId(pac2200ThingClassId)) {
                auto monitor = m_monitors.value(thing);
                if (!monitor->reachable()) {
                    continue;
                }

                auto connection = m_pac220TcpConnections.value(thing);
                if (connection->reachable()) {
                    qCDebug(dcSiemens()) << "Updating connection";
                    connection->update();
                } else {
                    qCDebug(dcSiemens()) << "Device not reachable. Probably a TCP connection error. Reconnecting TCP socket";
                    connection->reconnectDevice();
                }
            }
        });

        qCDebug(dcSiemens()) << "Starting plugin timer...";
        m_pluginTimer->start();
    }
}

void IntegrationPluginSiemens::thingRemoved(Thing *thing)
{
    qCDebug(dcSiemens()) << "Thing removed" << thing->name();

    if (m_pac220TcpConnections.contains(thing)) {
        auto connection = m_pac220TcpConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_pluginTimer) {
        qCDebug(dcSiemens()) << "Stopping plugin timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
