/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationpluginsolaxonly.h"
#include "plugininfo.h"
#include "discoverytcp.h"
#include "discoveryIES.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QEventLoop>

IntegrationPluginSolax::IntegrationPluginSolax()
{

}

void IntegrationPluginSolax::init()
{
    // Create error logging file
    QFile errorCounterFile("/data/solax-counter.txt");
    if (!errorCounterFile.exists()) {
        errorCounterFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QByteArray counter("Init: 0\nCont: 0");
        errorCounterFile.write(counter);
    }
    errorCounterFile.close();
}

void IntegrationPluginSolax::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxX3UltraThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolaxOnly()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        DiscoveryTcp *discovery = new DiscoveryTcp(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &DiscoveryTcp::discoveryFinished, info, [=](){
            foreach (const DiscoveryTcp::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(solaxX3UltraThingClassId, result.manufacturerName + " Inverter " + result.productName, QString("rated power: %1").arg(result.powerRating) + "W - " + result.networkDeviceInfo.address().toString());
                qCInfo(dcSolaxOnly()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxX3UltraThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolaxOnly()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(solaxX3UltraThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
                params << Param(solaxX3UltraThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(solaxX3UltraThingPortParamTypeId, result.port);
                params << Param(solaxX3UltraThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();
    } else if (info->thingClassId() == solaxIESInverterTCPThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolaxOnly()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        DiscoveryIes *discovery = new DiscoveryIes(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &DiscoveryIes::discoveryFinished, info, [=](){
            foreach (const DiscoveryIes::Result &result, discovery->discoveryResults()) {

                QString name = supportedThings().findById(solaxIESInverterTCPThingClassId).displayName();
                ThingDescriptor descriptor(solaxIESInverterTCPThingClassId, name, QString("rated power: %1").arg(result.powerRating) + "W - " + result.networkDeviceInfo.address().toString());
                qCInfo(dcSolaxOnly()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxIESInverterTCPThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolaxOnly()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(solaxIESInverterTCPThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
                params << Param(solaxIESInverterTCPThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(solaxIESInverterTCPThingPortParamTypeId, result.port);
                params << Param(solaxIESInverterTCPThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();
    }
}

void IntegrationPluginSolax::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSolaxOnly()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == solaxX3UltraThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSolaxOnly()) << "Already have a Solax connection for this thing. Cleaning up old connection and initializing new one...";
            SolaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolaxOnly()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is possible.
        // Testing for null is necessary, because registering a monitor with a zero mac adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue(solaxX3UltraThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolaxOnly()) << "Failed to set up Solax inverter because the MAC address is not valid:" << thing->paramValue(solaxX3UltraThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own.
        // In this call, nymea is checking a list for known mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip address and is set to not reachable
        m_monitors.insert(thing, monitor);

        // If the ip address was not found in the cache, wait for the for the network discovery cache to be updated
        qCDebug(dcSolaxOnly()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(solaxX3UltraThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) 
        {
            setupTcpConnection(info);
        } else {
            connect(hardwareManager()->networkDeviceDiscovery(), &NetworkDeviceDiscovery::cacheUpdated, info, [this, info]() {
                if (!m_setupTcpConnectionRunning)
                {
                    m_setupTcpConnectionRunning = true;
                    setupTcpConnection(info);
                }
            });
        }
    }

    if (thing->thingClassId() == solaxIESInverterTCPThingClassId) {

        // Handle reconfigure
        if (m_iesConnections.contains(thing)) {
            qCDebug(dcSolaxOnly()) << "Already have a Solax connection for this thing. Cleaning up old connection and initializing new one...";
            SolaxIesModbusTcpConnection *connection = m_iesConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolaxOnly()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is possible.
        // Testing for null is necessary, because registering a monitor with a zero mac adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue(solaxIESInverterTCPThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolaxOnly()) << "Failed to set up Solax inverter because the MAC address is not valid:" << thing->paramValue(solaxIESInverterTCPThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own.
        // In this call, nymea is checking a list for known mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip address and is set to not reachable
        m_monitors.insert(thing, monitor);

        // If the ip address was not found in the cache, wait for the for the network discovery cache to be updated
        qCDebug(dcSolaxOnly()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(solaxIESInverterTCPThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) 
        {
            setupIesTcpConnection(info);
        } else {
            connect(hardwareManager()->networkDeviceDiscovery(), &NetworkDeviceDiscovery::cacheUpdated, info, [this, info]() {
                if (!m_setupTcpConnectionRunning)
                {
                    m_setupTcpConnectionRunning = true;
                    setupIesTcpConnection(info);
                }
            });
        }
    }

    if (thing->thingClassId() == solaxMeterThingClassId) {
        qCDebug(dcSolaxOnly()) << "Setting up Meter thing.";
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (parentThing->thingClassId() == solaxX3UltraThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxX3UltraConnectedStateTypeId).toBool());
            } else if (parentThing->thingClassId() == solaxIESInverterTCPThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxIESInverterTCPConnectedStateTypeId).toBool());
            }
        }
        return;
    }

    if (thing->thingClassId() == solaxMeterSecondaryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (parentThing->thingClassId() == solaxIESInverterTCPThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxIESInverterTCPConnectedStateTypeId).toBool());
            }
        }

        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
        return;
    }

    if (thing->thingClassId() == solaxUltraBatteryThingClassId) {
        qCDebug(dcSolaxOnly()) << "Setting up Battery 1 thing.";
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (m_batterystates.value(parentThing).bmsCommStatus && m_batterystates.value(parentThing).modbusReachable) {
                thing->setStateValue(solaxUltraBatteryConnectedStateTypeId, true);
            } else {
                thing->setStateValue(solaxUltraBatteryConnectedStateTypeId, false);
                thing->setStateValue(solaxUltraBatteryCurrentPowerStateTypeId, 0);
            }

            // Set capacity from parent parameter.
            if (parentThing->thingClassId() == solaxX3UltraThingClassId) {
                thing->setStateValue(solaxUltraBatteryCapacityStateTypeId, parentThing->paramValue(solaxX3UltraThingBatteryCapacityParamTypeId).toUInt());
            }

            BatteryStates batteryStates{};
            m_batterystates.insert(thing, batteryStates);
        }
        return;
    }

    if (thing->thingClassId() == solaxUltraBattery2ThingClassId) {
        qCDebug(dcSolaxOnly()) << "Setting up Battery 2 thing.";
        // Nothing to to here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (m_batterystates.value(parentThing).bmsCommStatus && m_batterystates.value(parentThing).modbusReachable) {
                thing->setStateValue(solaxUltraBattery2ConnectedStateTypeId, true);
            } else {
                thing->setStateValue(solaxUltraBattery2ConnectedStateTypeId, false);
                thing->setStateValue(solaxUltraBattery2CurrentPowerStateTypeId, 0);
            }

            if (parentThing->thingClassId() == solaxX3UltraThingClassId) {
                thing->setStateValue(solaxUltraBattery2CapacityStateTypeId, parentThing->paramValue(solaxX3UltraThingBattery2CapacityParamTypeId).toUInt());
            }

            BatteryStates batteryStates{};
            m_batterystates.insert(thing, batteryStates);
        }
        return;
    }

    if (thing->thingClassId() == solaxBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (m_batterystates.value(parentThing).bmsCommStatus && m_batterystates.value(parentThing).modbusReachable) {
                thing->setStateValue(solaxBatteryConnectedStateTypeId, true);
            } else {
                thing->setStateValue(solaxBatteryConnectedStateTypeId, false);
                thing->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
            }

            // Set capacity from parent parameter.
            if (parentThing->thingClassId() == solaxIESInverterTCPThingClassId) {
                thing->setStateValue(solaxBatteryCapacityStateTypeId, parentThing->paramValue(solaxIESInverterTCPThingBatteryCapacityParamTypeId).toUInt());
            }
            qCWarning(dcSolaxOnly()) << "Setting up Battery Timer";
            m_batteryPowerTimer = new QTimer(this);

            connect(m_batteryPowerTimer, &QTimer::timeout, thing, [this, thing, parentThing]() {
                uint batteryTimeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
                int powerToSet = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt();
                bool forceBattery = thing->stateValue(solaxBatteryEnableForcePowerStateTypeId).toBool();
                qCWarning(dcSolaxOnly()) << "Battery countdown timer timeout. Manuel mode is" << forceBattery;
                if (forceBattery) {
                    qCWarning(dcSolaxOnly()) << "Battery power should be" << powerToSet;
                    qCWarning(dcSolaxOnly()) << "Battery timeout should be" << batteryTimeout;
                    setBatteryPower(parentThing, powerToSet, batteryTimeout);
                } else {
                    disableRemoteControl(parentThing);
                }
            });
        }

        return;
    }
}

void IntegrationPluginSolax::setupTcpConnection(ThingSetupInfo *info)
{
        qCDebug(dcSolaxOnly()) << "Setup TCP connection.";
        Thing *thing = info->thing();
        NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
        uint port = thing->paramValue(solaxX3UltraThingPortParamTypeId).toUInt();
        quint16 modbusId = thing->paramValue(solaxX3UltraThingModbusIdParamTypeId).toUInt();
        SolaxModbusTcpConnection *connection = new SolaxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, modbusId, this);
        m_tcpConnections.insert(thing, connection);
        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
        BatteryStates batteryStates{};
        m_batterystates.insert(thing, batteryStates);

        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Is this needed? How can setup be aborted at this point?

            // Clean up in case the setup gets aborted.
            if (m_monitors.contains(thing)) {
                qCDebug(dcSolaxOnly()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // Reconnect on monitor reachable changed.
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcSolaxOnly()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (reachable && !thing->stateValue(solaxX3UltraConnectedStateTypeId).toBool()) {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->reconnectDevice();
            } else if (!reachable) {
                // Note: We disable autoreconnect explicitly and we will
                // connect the device once the monitor says it is reachable again.
                connection->disconnectDevice();
            }
        });

        connect(connection, &SolaxModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable){
            qCDebug(dcSolaxOnly()) << "Reachable state changed" << reachable;
            if (reachable) {
                // Connected true will be set after successfull init.
                connection->initialize();
            } else {
                thing->setStateValue(solaxX3UltraConnectedStateTypeId, false);
                thing->setStateValue(solaxX3UltraCurrentPowerStateTypeId, 0);
                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }

                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    batteryThings.first()->setStateValue(solaxUltraBatteryCurrentPowerStateTypeId, 0);
                }

                Things batteryThings2 = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
                if (!batteryThings2.isEmpty()) {
                    batteryThings2.first()->setStateValue(solaxUltraBattery2CurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue(solaxX3UltraConnectedStateTypeId, success);

            // Set connected state for meter
            qCDebug(dcSolaxOnly()) << "Setting connected state for meter.";
            m_meterstates.find(thing)->modbusReachable = success;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                if (success && m_meterstates.value(thing).meterCommStatus) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }

            // Set connected state for battery
            m_batterystates.find(thing)->modbusReachable = success;
            qCDebug(dcSolaxOnly()) << "Setting connected state for battery 1.";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (success && m_batterystates.value(thing).bmsCommStatus) {
                    batteryThings.first()->setStateValue(solaxUltraBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxUltraBatteryConnectedStateTypeId, false);
                }
            }

            Things batteryThings2 = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            qCDebug(dcSolaxOnly()) << "Setting connected state for battery 2.";
            if (!batteryThings2.isEmpty()) {
                if (success && m_batterystates.value(thing).bmsCommStatus) {
                    batteryThings2.first()->setStateValue(solaxUltraBattery2ConnectedStateTypeId, true);
                } else {
                    batteryThings2.first()->setStateValue(solaxUltraBattery2ConnectedStateTypeId, false);
                }
            }

            if (success) {
                qCDebug(dcSolaxOnly()) << "Solax inverter initialized.";
                thing->setStateValue(solaxX3UltraFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxX3UltraRatedPowerStateTypeId, connection->inverterType());
            } else {
                qCDebug(dcSolaxOnly()) << "Solax inverter initialization failed.";
                // Try once to reconnect the device
                connection->reconnectDevice();
            } 
        });


        // Handle property changed signals for inverter
        // connect(connection, &SolaxModbusTcpConnection::EoutPowerTotalChanged, thing, [thing, this](double inverterPower){
        //     qCDebug(dcSolaxOnly()) << "Inverter power changed" << inverterPower << "W";
        //     // https://consolinno.atlassian.net/wiki/spaces/~62f39a8532850ea2a3268713/pages/462848121/Bugfixing+Solax+EX3
        //    
        //     double batteryPower = 0;
        //     double batteryPower2 = 0;
        //     Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
        //     if (!batteryThings.isEmpty()) {
        //         batteryPower = batteryThings.first()->stateValue(solaxUltraBatteryCurrentPowerStateTypeId).toDouble();
        //     }

        //     Things batteryThings2 = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
        //     if (!batteryThings.isEmpty()) {
        //         batteryPower2 = batteryThings.first()->stateValue(solaxUltraBattery2CurrentPowerStateTypeId).toDouble();
        //     }
        //    
        //     if (batteryPower < 0)
        //     {
        //         // Battery is discharging
        //         batteryPower = 0;
        //     }

        //     if (batteryPower2 < 0)
        //     {
        //         // Batter 2 is discharging
        //         batteryPower2 = 0;
        //     }

        //     qCDebug(dcSolaxOnly()) << "Subtract from InverterPower";
        //     thing->setStateValue(solaxX3UltraCurrentPowerStateTypeId, -inverterPower-batteryPower-batteryPower2);
        // });

        connect(connection, &SolaxModbusTcpConnection::inverterVoltageChanged, thing, [thing](double inverterVoltage){
            qCDebug(dcSolaxOnly()) << "Inverter voltage changed" << inverterVoltage << "V";
            thing->setStateValue(solaxX3UltraInverterVoltageStateTypeId, inverterVoltage);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterCurrentChanged, thing, [thing](double inverterCurrent){
            qCDebug(dcSolaxOnly()) << "Inverter current changed" << inverterCurrent << "A";
            thing->setStateValue(solaxX3UltraInverterCurrentStateTypeId, inverterCurrent);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc1Changed, thing, [thing](double powerDc1){
            qCDebug(dcSolaxOnly()) << "Inverter DC1 power changed" << powerDc1 << "W";
            thing->setStateValue(solaxX3UltraPv1PowerStateTypeId, powerDc1);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage1Changed, thing, [thing](double pvVoltage1){
            qCDebug(dcSolaxOnly()) << "Inverter PV1 voltage changed" << pvVoltage1 << "V";
            thing->setStateValue(solaxX3UltraPv1VoltageStateTypeId, pvVoltage1);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent1Changed, thing, [thing](double pvCurrent1){
            qCDebug(dcSolaxOnly()) << "Inverter PV1 current changed" << pvCurrent1 << "A";
            thing->setStateValue(solaxX3UltraPv1CurrentStateTypeId, pvCurrent1);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc2Changed, thing, [thing](double powerDc2){
            qCDebug(dcSolaxOnly()) << "Inverter DC2 power changed" << powerDc2 << "W";
            thing->setStateValue(solaxX3UltraPv2PowerStateTypeId, powerDc2);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage2Changed, thing, [thing](double pvVoltage2){
            qCDebug(dcSolaxOnly()) << "Inverter PV2 voltage changed" << pvVoltage2 << "V";
            thing->setStateValue(solaxX3UltraPv2VoltageStateTypeId, pvVoltage2);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent2Changed, thing, [thing](double pvCurrent2){
            qCDebug(dcSolaxOnly()) << "Inverter PV2 current changed" << pvCurrent2 << "A";
            thing->setStateValue(solaxX3UltraPv2CurrentStateTypeId, pvCurrent2);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc3Changed, thing, [thing](double powerDc3){
            qCDebug(dcSolaxOnly()) << "Inverter DC3 power changed" << powerDc3 << "W";
            thing->setStateValue(solaxX3UltraPv3PowerStateTypeId, powerDc3);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage3Changed, thing, [thing](double pvVoltage3){
            qCDebug(dcSolaxOnly()) << "Inverter PV3 voltage changed" << pvVoltage3 << "V";
            thing->setStateValue(solaxX3UltraPv3VoltageStateTypeId, pvVoltage3);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent3Changed, thing, [thing](double pvCurrent3){
            qCDebug(dcSolaxOnly()) << "Inverter PV3 current changed" << pvCurrent3 << "A";
            thing->setStateValue(solaxX3UltraPv3CurrentStateTypeId, pvCurrent3);
        });

        connect(connection, &SolaxModbusTcpConnection::runModeChanged, thing, [this, thing](SolaxModbusTcpConnection::RunMode runMode){
            qCDebug(dcSolaxOnly()) << "Inverter run mode recieved" << runMode;
            quint16 runModeAsInt = runMode;
            setRunMode(thing, runModeAsInt);
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureChanged, thing, [thing](quint16 temperature){
            qCDebug(dcSolaxOnly()) << "Inverter temperature changed" << temperature << "°C";
            thing->setStateValue(solaxX3UltraTemperatureStateTypeId, temperature);
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTotalChanged, thing, [thing](double solarEnergyTotal){
            qCDebug(dcSolaxOnly()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";
            thing->setStateValue(solaxX3UltraTotalEnergyProducedStateTypeId, solarEnergyTotal);
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolaxOnly()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxX3UltraEnergyProducedTodayStateTypeId, solarEnergyToday);
        });

        connect(connection, &SolaxModbusTcpConnection::activePowerLimitChanged, thing, [thing](quint16 activePowerLimit){
            qCDebug(dcSolaxOnly()) << "Inverter active power limit changed" << activePowerLimit << "%";
            thing->setStateValue(solaxX3UltraActivePowerLimitStateTypeId, activePowerLimit);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFaultBitsChanged, thing, [this, thing](quint32 inverterFaultBits){
            qCDebug(dcSolaxOnly()) << "Inverter fault bits recieved" << inverterFaultBits;
            setErrorMessage(thing, inverterFaultBits);
        });


        // Meter
        connect(connection, &SolaxModbusTcpConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            m_meterstates.find(thing)->meterCommStatus = commStatusBool;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(thing).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinPowerChanged, thing, [this, thing](qint32 feedinPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                // Sign should be correct, but check to make sure.
                meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -double(feedinPower));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentRChanged, thing, [this, thing](double currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A current (gridCurrentR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentSChanged, thing, [this, thing](double currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B current (gridCurrentS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentTChanged, thing, [this, thing](double currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C current (gridCurrentT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerRChanged, thing, [this, thing](qint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A power (gridPowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseAStateTypeId, double(currentPowerPhaseA));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerSChanged, thing, [this, thing](qint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B power (gridPowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseBStateTypeId, double(currentPowerPhaseB));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C power (gridPowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseCStateTypeId, double(currentPowerPhaseC));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageRChanged, thing, [this, thing](double voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A voltage (gridVoltageR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageSChanged, thing, [this, thing](double voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B voltage (gridVoltageS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageTChanged, thing, [this, thing](double voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C voltage (gridVoltageT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencyRChanged, thing, [this, thing](double gridFrequencyR){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A frequency (gridFrequencyR) changed" << gridFrequencyR << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseAStateTypeId, gridFrequencyR);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencySChanged, thing, [this, thing](double gridFrequencyS){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B frequency (gridFrequencyS) changed" << gridFrequencyS << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseBStateTypeId, gridFrequencyS);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencyTChanged, thing, [this, thing](double gridFrequencyT){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C frequency (gridFrequencyT) changed" << gridFrequencyT << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseCStateTypeId, gridFrequencyT);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFrequencyChanged, thing, [this, thing](double frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter frequency (gridFrequency) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyStateTypeId, frequency);
            }
        });


        // Battery        
        connect(connection, &SolaxModbusTcpConnection::bmsConnectStateChanged, thing, [this, thing, connection](quint16 bmsConnect){
            // TODO: create BMS2 thing
            // Debug output even if there is no battery thing, since this signal creates it.
            qCDebug(dcSolaxOnly()) << "Battery connect state (bmsConnectState) changed" << bmsConnect;
            bool bmsCommStatusBool = (bmsConnect != 0);
            m_batterystates.find(thing)->bmsCommStatus = bmsCommStatusBool;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (bmsCommStatusBool && m_batterystates.value(thing).modbusReachable) {
                    batteryThings.first()->setStateValue(solaxUltraBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxUltraBatteryConnectedStateTypeId, false);
                }
            } else if (bmsCommStatusBool){
                // Battery detected. No battery exists yet. Create it.
                qCDebug(dcSolaxOnly()) << "Set up Solax battery for" << thing;
                ThingDescriptor descriptor(solaxUltraBatteryThingClassId, "Solax battery", QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            Things batteryThings2 = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings2.isEmpty()) {
                if (bmsCommStatusBool && m_batterystates.value(thing).modbusReachable) {
                    batteryThings2.first()->setStateValue(solaxUltraBattery2ConnectedStateTypeId, true);
                } else {
                    batteryThings2.first()->setStateValue(solaxUltraBattery2ConnectedStateTypeId, false);
                }
            } else if (connection->temperatureBat2() != 0) {
                // Temperature of Bat 2 is unlikely 0 if it is connected
                qCDebug(dcSolaxOnly()) << "Set up Solax battery 2 for" << thing;
                ThingDescriptor descriptor(solaxUltraBattery2ThingClassId, "Solax battery 2", QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batPowerCharge1Changed, thing, [this, thing](qint16 powerBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery power (batpowerCharge1) changed" << powerBat1 << "W";

                // ToDo: Check if sign for charge / dischage is correct.
                batteryThings.first()->setStateValue(solaxUltraBatteryCurrentPowerStateTypeId, double(powerBat1));
                if (powerBat1 < 0) {
                    batteryThings.first()->setStateValue(solaxUltraBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat1 > 0) {
                    batteryThings.first()->setStateValue(solaxUltraBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(solaxUltraBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        // connect(connection, &SolaxModbusTcpConnection::batteryCapacityChanged, thing, [this, thing](quint16 socBat1){
        //     Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
        //     if (!batteryThings.isEmpty()) {
        //         qCDebug(dcSolaxOnly()) << "Battery state of charge (batteryCapacity) changed" << socBat1 << "%";
        //         batteryThings.first()->setStateValue(solaxUltraBatteryBatteryLevelStateTypeId, socBat1);
        //         batteryThings.first()->setStateValue(solaxUltraBatteryBatteryCriticalStateTypeId, socBat1 < 10);
        //     }
        // });

        connect(connection, &SolaxModbusTcpConnection::bmsWarningLsbChanged, thing, [this, thing](quint16 batteryWarningBitsLsb){
            qCDebug(dcSolaxOnly()) << "Battery warning bits LSB recieved" << batteryWarningBitsLsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty())
            {
                Thing* batteryThing = batteryThings.first();
                m_batterystates.find(batteryThing)->bmsWarningLsb = batteryWarningBitsLsb;
                setBmsWarningMessage(batteryThing);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::bmsWarningMsbChanged, thing, [this, thing](quint16 batteryWarningBitsMsb){
            qCDebug(dcSolaxOnly()) << "Battery warning bits MSB recieved" << batteryWarningBitsMsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty())
            {
                Thing* batteryThing = batteryThings.first();
                m_batterystates.find(batteryThing)->bmsWarningMsb = batteryWarningBitsMsb;
                setBmsWarningMessage(batteryThing);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batVoltageCharge1Changed, thing, [this, thing](double batVoltageCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery voltage changed" << batVoltageCharge1 << "V";
                batteryThings.first()->setStateValue(solaxUltraBatteryBatteryVoltageStateTypeId, batVoltageCharge1);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batCurrentCharge1Changed, thing, [this, thing](double batCurrentCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery current changed" << batCurrentCharge1 << "A";
                batteryThings.first()->setStateValue(solaxUltraBatteryBatteryCurrentStateTypeId, batCurrentCharge1);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureBatChanged, thing, [this, thing](quint16 temperatureBat){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery temperature changed" << temperatureBat << "°C";
                batteryThings.first()->setStateValue(solaxUltraBatteryTemperatureStateTypeId, temperatureBat);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batVoltageCharge2Changed, thing, [this, thing](double batVoltCharge2) {
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Batter 2 voltage changed" << batVoltCharge2 << "W";
                batteryThings.first()->setStateValue(solaxUltraBattery2BatteryVoltageStateTypeId, batVoltCharge2);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batCurrentCharge2Changed, thing, [this, thing](double batCurrentCharge2){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery 2 current changed" << batCurrentCharge2 << "A";
                batteryThings.first()->setStateValue(solaxUltraBattery2BatteryCurrentStateTypeId, batCurrentCharge2);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureBat2Changed, thing, [this, thing](qint16 bat2Temperature) {
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Batter 2 temperature changed" << bat2Temperature << "°C";
                batteryThings.first()->setStateValue(solaxUltraBattery2TemperatureStateTypeId, bat2Temperature);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batPowerCharge2Changed, thing, [this, thing](qint16 powerBat2){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery power (batpowerCharge2) changed" << powerBat2 << "W";

                // ToDo: Check if sign for charge / dischage is correct.
                batteryThings.first()->setStateValue(solaxUltraBattery2CurrentPowerStateTypeId, double(powerBat2));
                if (powerBat2 < 0) {
                    batteryThings.first()->setStateValue(solaxUltraBattery2ChargingStateStateTypeId, "discharging");
                } else if (powerBat2 > 0) {
                    batteryThings.first()->setStateValue(solaxUltraBattery2ChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(solaxUltraBattery2ChargingStateStateTypeId, "idle");
                }
            }
        });

        // connect(connection, &SolaxModbusTcpConnection::batteryCapacity2Changed, thing, [this, thing](quint16 socBat2){
        //     Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
        //     if (!batteryThings.isEmpty()) {
        //         qCDebug(dcSolaxOnly()) << "Battery 2 state of charge (batteryCapacity) changed" << socBat2 << "%";
        //         batteryThings.first()->setStateValue(solaxUltraBattery2BatteryLevelStateTypeId, socBat2);
        //         batteryThings.first()->setStateValue(solaxUltraBattery2BatteryCriticalStateTypeId, socBat2 < 10);
        //     }
        // });

        connect(connection, &SolaxModbusTcpConnection::bms2FaultLsbChanged, thing, [this, thing](quint16 batteryWarningBitsLsb){
            qCDebug(dcSolaxOnly()) << "Battery 2 warning bits LSB recieved" << batteryWarningBitsLsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings.isEmpty())
            {
                Thing* batteryThing = batteryThings.first();
                m_batterystates.find(batteryThing)->bmsWarningLsb = batteryWarningBitsLsb;
                setBmsWarningMessage(batteryThing);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::bms2FaulMLsbChanged, thing, [this, thing](quint16 batteryWarningBitsMsb){
            qCDebug(dcSolaxOnly()) << "Battery 2 warning bits MSB recieved" << batteryWarningBitsMsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings.isEmpty())
            {
                Thing* batteryThing = batteryThings.first();
                m_batterystates.find(batteryThing)->bmsWarningMsb = batteryWarningBitsMsb;
                setBmsWarningMessage(batteryThing);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::updateFinished, thing, [this, thing, connection]() {
            qCDebug(dcSolaxOnly()) << "Calculate total inverter power";
            quint16 powerDc1 = connection->powerDc1();
            quint16 powerDc2 = connection->powerDc2();
            quint16 powerDc3 = connection->powerDc3();
            thing->setStateValue(solaxX3UltraCurrentPowerStateTypeId, -(powerDc1+powerDc2+powerDc3));

            // Set battery 1 state of charge
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                quint16 socBat1 = connection->batteryCapacity();
                qCDebug(dcSolaxOnly()) << "Battery state of charge (batteryCapacity) changed" << socBat1 << "%";
                batteryThings.first()->setStateValue(solaxUltraBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(solaxUltraBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            }

            // Set battery 2 state of charge
            Things batteryThings2 = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxUltraBattery2ThingClassId);
            if (!batteryThings2.isEmpty()) {
                quint16 socBat2 = connection->batteryCapacity2();
                qCDebug(dcSolaxOnly()) << "Battery 2 state of charge (batteryCapacity) changed" << socBat2 << "%";
                batteryThings2.first()->setStateValue(solaxUltraBattery2BatteryLevelStateTypeId, socBat2);
                batteryThings2.first()->setStateValue(solaxUltraBattery2BatteryCriticalStateTypeId, socBat2 < 10);
            }
        });


        if (monitor->reachable())
            connection->connectDevice();

        info->finish(Thing::ThingErrorNoError);

        return;
}

void IntegrationPluginSolax::setupIesTcpConnection(ThingSetupInfo *info)
{
        qCDebug(dcSolaxOnly()) << "Setup IES TCP connection.";
        Thing *thing = info->thing();
        NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
        uint port = thing->paramValue(solaxIESInverterTCPThingPortParamTypeId).toUInt();
        quint16 modbusId = thing->paramValue(solaxIESInverterTCPThingModbusIdParamTypeId).toUInt();
        SolaxIesModbusTcpConnection *connection = new SolaxIesModbusTcpConnection(monitor->networkDeviceInfo().address(), port, modbusId, this);
        m_iesConnections.insert(thing, connection);
        BatteryStates batteryStates{};
        m_batterystates.insert(thing, batteryStates);

        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Is this needed? How can setup be aborted at this point?

            // Clean up in case the setup gets aborted.
            if (m_monitors.contains(thing)) {
                qCDebug(dcSolaxOnly()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // Reconnect on monitor reachable changed.
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcSolaxOnly()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (reachable && !thing->stateValue(solaxIESInverterTCPConnectedStateTypeId).toBool()) {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->reconnectDevice();
            } else if (!reachable) {
                // Note: We disable autoreconnect explicitly and we will
                // connect the device once the monitor says it is reachable again.
                connection->disconnectDevice();
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable){
            qCDebug(dcSolaxOnly()) << "Reachable state changed" << reachable;
            if (reachable) {
                // Connected true will be set after successfull init.
                connection->initialize();
            } else {
                thing->setStateValue(solaxIESInverterTCPConnectedStateTypeId, false);
                thing->setStateValue(solaxIESInverterTCPCurrentPowerStateTypeId, 0);
                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                    childThing->setStateValue("currentPower", 0);
                }

                // Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
                // if (!meterThings.isEmpty()) {
                //     meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                //     meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                // }

                // Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
                // if (!batteryThings.isEmpty()) {
                //     batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
                //     batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                // }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue(solaxIESInverterTCPConnectedStateTypeId, success);
            m_energyCheck = 500;
            writePasswordToInverter(thing);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->modbusReachable = success;
                if (success && m_meterstates.value(meterThings.first()).meterCommStatus) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }

            Things secondaryMeterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!secondaryMeterThings.isEmpty()) {
                m_meterstates.find(secondaryMeterThings.first())->modbusReachable = success;
                if (success && m_meterstates.value(secondaryMeterThings.first()).meterCommStatus) {
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, true);
                } else {
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
                }
            }

            // Set connected state for battery
            m_batterystates.find(thing)->modbusReachable = success;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (success && m_batterystates.value(thing).bmsCommStatus) {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                }
            }

            if (success) {
                qCDebug(dcSolaxOnly()) << "Solax inverter initialized.";
                thing->setStateValue(solaxIESInverterTCPFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxIESInverterTCPNominalPowerStateTypeId, connection->inverterType());
                // thing->setStateValue(solaxIESInverterTCPExportLimitStateTypeId, connection->inverterType());
            } else {
                qCDebug(dcSolaxOnly()) << "Solax inverter initialization failed.";
                // Try once to reconnect the device
                connection->reconnectDevice();
            } 
        });

        connect(connection, &SolaxIesModbusTcpConnection::inverterVoltageChanged, thing, [thing](double inverterVoltage){
            qCDebug(dcSolaxOnly()) << "Inverter voltage changed" << inverterVoltage << "V";
            thing->setStateValue(solaxIESInverterTCPInverterVoltageStateTypeId, inverterVoltage);
        });

        connect(connection, &SolaxIesModbusTcpConnection::inverterCurrentChanged, thing, [thing](double inverterCurrent){
            qCDebug(dcSolaxOnly()) << "Inverter current changed" << inverterCurrent << "A";
            thing->setStateValue(solaxIESInverterTCPInverterCurrentStateTypeId, inverterCurrent);
        });

        connect(connection, &SolaxIesModbusTcpConnection::powerDc1Changed, thing, [thing](double powerDc1){
            qCDebug(dcSolaxOnly()) << "Inverter DC1 power changed" << powerDc1 << "W";
            thing->setStateValue(solaxIESInverterTCPPv1PowerStateTypeId, powerDc1);
        });

        connect(connection, &SolaxIesModbusTcpConnection::pvVoltage1Changed, thing, [thing](double pvVoltage1){
            qCDebug(dcSolaxOnly()) << "Inverter PV1 voltage changed" << pvVoltage1 << "V";
            thing->setStateValue(solaxIESInverterTCPPv1VoltageStateTypeId, pvVoltage1);
        });

        connect(connection, &SolaxIesModbusTcpConnection::pvCurrent1Changed, thing, [thing](double pvCurrent1){
            qCDebug(dcSolaxOnly()) << "Inverter PV1 current changed" << pvCurrent1 << "A";
            thing->setStateValue(solaxIESInverterTCPPv1CurrentStateTypeId, pvCurrent1);
        });

        connect(connection, &SolaxIesModbusTcpConnection::powerDc2Changed, thing, [thing](double powerDc2){
            qCDebug(dcSolaxOnly()) << "Inverter DC2 power changed" << powerDc2 << "W";
            thing->setStateValue(solaxIESInverterTCPPv2PowerStateTypeId, powerDc2);
        });

        connect(connection, &SolaxIesModbusTcpConnection::pvVoltage2Changed, thing, [thing](double pvVoltage2){
            qCDebug(dcSolaxOnly()) << "Inverter PV2 voltage changed" << pvVoltage2 << "V";
            thing->setStateValue(solaxIESInverterTCPPv2VoltageStateTypeId, pvVoltage2);
        });

        connect(connection, &SolaxIesModbusTcpConnection::pvCurrent2Changed, thing, [thing](double pvCurrent2){
            qCDebug(dcSolaxOnly()) << "Inverter PV2 current changed" << pvCurrent2 << "A";
            thing->setStateValue(solaxIESInverterTCPPv2CurrentStateTypeId, pvCurrent2);
        });

        connect(connection, &SolaxIesModbusTcpConnection::runModeChanged, thing, [this, thing](SolaxIesModbusTcpConnection::RunMode runMode){
            qCDebug(dcSolaxOnly()) << "Inverter run mode recieved" << runMode;
            quint16 runModeAsInt = runMode;
            setRunMode(thing, runModeAsInt);
        });

        connect(connection, &SolaxIesModbusTcpConnection::temperatureChanged, thing, [thing](quint16 temperature){
            qCDebug(dcSolaxOnly()) << "Inverter temperature changed" << temperature << "°C";
            thing->setStateValue(solaxIESInverterTCPTemperatureStateTypeId, temperature);
        });

        connect(connection, &SolaxIesModbusTcpConnection::solarEnergyTotalChanged, thing, [this, thing](double solarEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            qCDebug(dcSolaxOnly()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";

            double oldEnergyValue = thing->stateValue(solaxIESInverterTCPTotalEnergyProducedStateTypeId).toDouble();
            double diffEnergy = solarEnergyTotal - oldEnergyValue;
            if (oldEnergyValue == 0 ||
                (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
            {
                thing->setStateValue(solaxIESInverterTCPTotalEnergyProducedStateTypeId, solarEnergyTotal);
            } else {
                qCWarning(dcSolaxOnly()) << "TCP Inverter - Old Energy value is" << oldEnergyValue;
                qCWarning(dcSolaxOnly()) << "TCP Inverter - New energy value is" << solarEnergyTotal;
                writeErrorLog();
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolaxOnly()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxIESInverterTCPEnergyProducedTodayStateTypeId, solarEnergyToday);
        });

        connect(connection, &SolaxIesModbusTcpConnection::readExportLimitChanged, thing, [thing](float limit){
            qCWarning(dcSolaxOnly()) << "Export limit changed to " << limit << "W";
            thing->setStateValue(solaxIESInverterTCPExportLimitStateTypeId, limit);
        });

        connect(connection, &SolaxIesModbusTcpConnection::modbusPowerControlChanged, thing, [this, thing](quint16 controlMode) {
            qCWarning(dcSolaxOnly()) << "Modbus power control changed to" << controlMode;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (controlMode == 4) {
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, false);
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::updateFinished, thing, [this, thing, connection](){
            qCDebug(dcSolaxOnly()) << "Solax X3 - Update finished.";

            // TODO: does this reliably create the secondary meter?
            Things secondaryMeterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);

            quint16 meter2CommState = connection->meter2CommunicationState();
            if (meter2CommState == 1 && meter2CommState == m_secondMeterCheck) {
                m_secondMeterCounter++;
            } else {
                m_secondMeterCounter = 0;
            }
            m_secondMeterCheck = meter2CommState;

            if (secondaryMeterThings.isEmpty() && m_secondMeterCounter == 3) {
                qCDebug(dcSolaxOnly()) << "Create the secondary meter thing";
                QString name = supportedThings().findById(solaxMeterSecondaryThingClassId).displayName();
                ThingDescriptor descriptor(solaxMeterSecondaryThingClassId, name, QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            qint16 currentBatPower = 0;
            if (!batteryThings.isEmpty()) {
                currentBatPower = batteryThings.first()->stateValue(solaxBatteryCurrentPowerStateTypeId).toInt();
                if (m_batteryPowerTimer->isActive()) {
                    batteryThings.first()->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, (int) m_batteryPowerTimer->remainingTime()/1000);
                }

                float batMaxVoltage = connection->batMaxDischargeVolt();
                float batMaxCurrent = connection->batMaxDischargeCurrent();
                double batMaxPower = batMaxCurrent*batMaxVoltage;
                // It seems batMaxVoltage and matMaxCurrent are not always provided by the system
                if (batMaxPower == 0)
                    batMaxPower = 10000;

                batteryThings.first()->setStateValue(solaxBatteryNominalPowerBatteryStateTypeId, batMaxPower);

                // check if manual is following the chosen setting
                bool userSetState = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateTypeId).toBool();
                bool actualState = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
                if (userSetState != actualState) {
                    if (userSetState == true) {
                        uint batteryTimeout = batteryThings.first()->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
                        int powerToSet = batteryThings.first()->stateValue(solaxBatteryForcePowerStateTypeId).toInt();
                        setBatteryPower(thing, powerToSet, batteryTimeout);
                    } else {
                        disableRemoteControl(thing);
                    }
                }
            }

            qCDebug(dcSolaxOnly()) << "Set inverter power";
            quint16 powerDc1 = connection->powerDc1();
            quint16 powerDc2 = connection->powerDc2();
            // If battery is currently charging, check if batteryPower is bigger than solaxpower
            // If yes, add that difference to solarpower
            qint16 powerDifference = 0;
            if (currentBatPower > 0 && (powerDc1+powerDc2) > 0) {
                powerDifference = currentBatPower - (powerDc1+powerDc2);
                if (powerDifference < 0) {
                    powerDifference = 0;
                }
            }
            double nominalPowerInverter = thing->stateValue(solaxIESInverterTCPNominalPowerStateTypeId).toDouble();
            double currentPower = powerDc1+powerDc2+2*powerDifference;
            if (qFabs(currentPower) < nominalPowerInverter + 5000)
                thing->setStateValue(solaxIESInverterTCPCurrentPowerStateTypeId, -(powerDc1+powerDc2+powerDifference/10));

            m_energyCheck = 10;
        });

        // Meter
        connect(connection, &SolaxIesModbusTcpConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->meterCommStatus = commStatusBool;
                qCDebug(dcSolaxOnly()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(meterThings.first()).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::meter2CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->meterCommStatus = commStatusBool;
                qCDebug(dcSolaxOnly()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(meterThings.first()).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterSecondaryCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::feedinPowerChanged, thing, [this, thing](qint32 feedinPower){
            double nominalPowerInverter = thing->stateValue(solaxIESInverterTCPNominalPowerStateTypeId).toDouble();
            double nominalPowerBattery = 0;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                // Get max power of the battery
                nominalPowerBattery = batteryThings.first()->stateValue(solaxBatteryNominalPowerBatteryStateTypeId).toDouble();
            }

            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                if (qFabs(feedinPower) < (nominalPowerInverter + nominalPowerBattery + 1000))
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -1 * static_cast<double>(feedinPower));
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterTotalEnergyProducedStateTypeId).toDouble();
                double diffEnergy = feedinEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
                } else {
                    qCWarning(dcSolaxOnly()) << "TCP Meter Produced - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolaxOnly()) << "TCP Meter Produced - New energy value is" << feedinEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            // New value should not be smaller then old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = consumEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
                } else {
                    qCWarning(dcSolaxOnly()) << "TCP Meter Consumed - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolaxOnly()) << "TCP Meter Consumed - New energy value is" << consumEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridCurrentRChanged, thing, [this, thing](double currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A current (gridCurrentR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridCurrentSChanged, thing, [this, thing](double currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B current (gridCurrentS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridCurrentTChanged, thing, [this, thing](double currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C current (gridCurrentT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridPowerRChanged, thing, [this, thing](qint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A power (gridPowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseAStateTypeId, double(currentPowerPhaseA));
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridPowerSChanged, thing, [this, thing](qint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B power (gridPowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseBStateTypeId, double(currentPowerPhaseB));
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridPowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C power (gridPowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseCStateTypeId, double(currentPowerPhaseC));
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridVoltageRChanged, thing, [this, thing](double voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A voltage (gridVoltageR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridVoltageSChanged, thing, [this, thing](double voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B voltage (gridVoltageS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridVoltageTChanged, thing, [this, thing](double voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C voltage (gridVoltageT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridFrequencyRChanged, thing, [this, thing](double gridFrequencyR){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase A frequency (gridFrequencyR) changed" << gridFrequencyR << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseAStateTypeId, gridFrequencyR);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridFrequencySChanged, thing, [this, thing](double gridFrequencyS){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase B frequency (gridFrequencyS) changed" << gridFrequencyS << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseBStateTypeId, gridFrequencyS);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::gridFrequencyTChanged, thing, [this, thing](double gridFrequencyT){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter phase C frequency (gridFrequencyT) changed" << gridFrequencyT << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseCStateTypeId, gridFrequencyT);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::inverterFrequencyChanged, thing, [this, thing](double frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter frequency (gridFrequency) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyStateTypeId, frequency);
            }
        });


        // Battery        
        connect(connection, &SolaxIesModbusTcpConnection::bmsConnectStateChanged, thing, [this, thing](quint16 bmsConnect){
            // Debug output even if there is no battery thing, since this signal creates it.
            qCDebug(dcSolaxOnly()) << "Battery connect state (bmsConnectState) changed" << bmsConnect;
            bool bmsCommStatusBool = (bmsConnect != 0);
            m_batterystates.find(thing)->bmsCommStatus = bmsCommStatusBool;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (bmsCommStatusBool && m_batterystates.value(thing).modbusReachable) {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                }
            } else if (bmsCommStatusBool){
                // Battery detected. No battery exists yet. Create it.
                qCDebug(dcSolaxOnly()) << "Set up Solax battery for" << thing;
                QString name = supportedThings().findById(solaxBatteryThingClassId).displayName();
                ThingDescriptor descriptor(solaxBatteryThingClassId, name, QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::batPowerCharge1Changed, thing, [this, thing](qint16 powerBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery power (batpowerCharge1) changed" << powerBat1 << "W";

                double nominalPowerBattery = batteryThings.first()->stateValue(solaxBatteryNominalPowerBatteryStateTypeId).toDouble();
                if (qFabs(powerBat1) < nominalPowerBattery + 5000)
                    batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, double(powerBat1));

                if (powerBat1 < 0) {
                    batteryThings.first()->setStateValue(solaxBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat1 > 0) {
                    batteryThings.first()->setStateValue(solaxBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::batteryCapacityChanged, thing, [this, thing](quint16 socBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery state of charge (batteryCapacity) changed" << socBat1 << "%";

                quint16 oldBatteryLevel = batteryThings.first()->stateValue(solaxBatteryBatteryLevelStateTypeId).toUInt();
                quint16 batteryLevelDiff = qFabs(socBat1-oldBatteryLevel);
                if (oldBatteryLevel == 0 || batteryLevelDiff <= 10)
                    batteryThings.first()->setStateValue(solaxBatteryBatteryLevelStateTypeId, socBat1);

                batteryThings.first()->setStateValue(solaxBatteryBatteryCriticalStateTypeId, socBat1 < 10);
                int minBatteryLevel = batteryThings.first()->stateValue(solaxBatteryMinBatteryLevelStateTypeId).toInt();
                bool batManualMode = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
                if (socBat1 <= minBatteryLevel && batManualMode == true) {
                    qCWarning(dcSolaxOnly()) << "Batter level below set minimum value";
                    disableRemoteControl(thing);
                    // batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, false);
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateTypeId, false);
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::batVoltageCharge1Changed, thing, [this, thing](double batVoltageCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery voltage changed" << batVoltageCharge1 << "V";
                batteryThings.first()->setStateValue(solaxBatteryBatteryVoltageStateTypeId, batVoltageCharge1);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::batCurrentCharge1Changed, thing, [this, thing](double batCurrentCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery current changed" << batCurrentCharge1 << "A";
                batteryThings.first()->setStateValue(solaxBatteryBatteryCurrentStateTypeId, batCurrentCharge1);
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::temperatureBatChanged, thing, [this, thing](quint16 temperatureBat){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Battery temperature changed" << temperatureBat << "°C";
                batteryThings.first()->setStateValue(solaxBatteryTemperatureStateTypeId, temperatureBat);
            }
        });

        // Set meter 2 states
        connect(connection, &SolaxIesModbusTcpConnection::consumEnergyTotalMeter2Changed, thing, [this, thing](double consumEnergyTotal){
            // New value should not be smaller then old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterSecondaryTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = consumEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterSecondaryTotalEnergyConsumedStateTypeId, 0);
                } else {
                    qCWarning(dcSolaxOnly()) << "TCP Meter 2 Consumed - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolaxOnly()) << "TCP Meter 2 Consumed - New energy value is" << consumEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::feedinEnergyTotalMeter2Changed, thing, [this, thing](double feedinEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterSecondaryTotalEnergyProducedStateTypeId).toDouble();
                double diffEnergy = feedinEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterSecondaryTotalEnergyProducedStateTypeId, feedinEnergyTotal);
                } else {
                    qCWarning(dcSolaxOnly()) << "TCP Meter Produced - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolaxOnly()) << "TCP Meter Produced - New energy value is" << feedinEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxIesModbusTcpConnection::currentPowerMeter2Changed, thing, [this, thing](qint32 feedinPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxOnly()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                double power = qFabs(static_cast<double>(feedinPower));
                if (power <= 20000)
                    meterThings.first()->setStateValue(solaxMeterSecondaryCurrentPowerStateTypeId, -1 * power);
            }
        });

        if (monitor->reachable())
            connection->connectDevice();

        info->finish(Thing::ThingErrorNoError);

        return;
}

void IntegrationPluginSolax::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == solaxX3UltraThingClassId ||
        thing->thingClassId() == solaxIESInverterTCPThingClassId) {

        bool connectionWasCreated = m_tcpConnections.contains(thing) ||
                                    m_iesConnections.contains(thing);
        if (!connectionWasCreated) {
            qCDebug(dcSolaxOnly()) << "Aborting post setup, because setup did not complete.";
            return;
        }

        if (!m_pluginTimer) {
            qCDebug(dcSolaxOnly()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SolaxModbusTcpConnection *connection, m_tcpConnections) {
                    connection->update();
                }
                foreach(SolaxIesModbusTcpConnection *connection, m_iesConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId).isEmpty()) {
            qCDebug(dcSolaxOnly()) << "Set up solax meter for" << thing;
            QString name = supportedThings().findById(solaxMeterThingClassId).displayName();
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(solaxMeterThingClassId, name, QString(), thing->id()));
        }
    }
}

void IntegrationPluginSolax::writePasswordToInverter(Thing *thing)
{
    if (thing->thingClassId() == solaxIESInverterTCPThingClassId)
    {
        qCDebug(dcSolaxOnly()) << "Set unlock password";
        SolaxIesModbusTcpConnection *connection = m_iesConnections.value(thing);
        QModbusReply *reply = connection->setUnlockPassword(2014);
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, thing, [this, thing, reply](){
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcSolaxOnly()) << "Error setting unlockpassword" << reply->error() << reply->errorString();
                writePasswordToInverter(thing);
            } else {
                qCWarning(dcSolaxOnly()) << "Successfully set unlock password";
            }
        });
    }
}

void IntegrationPluginSolax::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == solaxIESInverterTCPThingClassId) {
        SolaxIesModbusTcpConnection *connection = m_iesConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolaxOnly()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->connected()) {
            qCWarning(dcSolaxOnly()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxIESInverterTCPSetExportLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxIESInverterTCPSetExportLimitActionExportLimitParamTypeId).toUInt();
            double ratedPower = thing->stateValue(solaxIESInverterTCPNominalPowerStateTypeId).toDouble();
            qCWarning(dcSolaxOnly()) << "Rated power is" << ratedPower;
            qCWarning(dcSolaxOnly()) << "Trying to set active power limit to" << powerLimit;
            quint16 target = powerLimit * (ratedPower/100); 
            QModbusReply *reply = connection->setWriteExportLimit(target);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, thing, reply, powerLimit, target](){
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSolaxOnly()) << "Error setting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCWarning(dcSolaxOnly()) << "Active power limit set to" << target;
                    //thing->setStateValue(solaxIESInverterTCPExportLimitStateTypeId, target);
                    info->finish(Thing::ThingErrorNoError);
                }
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    } else if (thing->thingClassId() == solaxBatteryThingClassId) {
        Thing *inverterThing = myThings().findById(thing->parentId());
        qCWarning(dcSolaxOnly()) << "executeAction: should be inverter thing" << inverterThing;

        if (action.actionTypeId() == solaxBatteryEnableForcePowerActionTypeId) {
            bool state = action.paramValue(solaxBatteryEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
            uint batteryTimeout = thing->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
            int powerToSet = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt();

            qCWarning(dcSolaxOnly()) << "Battery manual mode is enabled?" << state;
            if (state) {
                setBatteryPower(inverterThing, powerToSet, batteryTimeout);
            } else {
                disableRemoteControl(inverterThing);
            }

            // thing->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, state);
            thing->setStateValue(solaxBatteryEnableForcePowerStateTypeId, state);
        } else if (action.actionTypeId() == solaxBatteryForcePowerActionTypeId) {
            int batteryPower = action.paramValue(solaxBatteryForcePowerActionForcePowerParamTypeId).toInt();
            qCWarning(dcSolaxOnly()) << "Battery power should be set to" << batteryPower;
            thing->setStateValue(solaxBatteryForcePowerStateTypeId, batteryPower);

            uint timeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
            bool state = thing->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
            if (state)
            {
                setBatteryPower(inverterThing, batteryPower, timeout);
            }
        } else if (action.actionTypeId() == solaxBatteryForcePowerTimeoutActionTypeId) {
            uint timeout = action.paramValue(solaxBatteryForcePowerTimeoutActionForcePowerTimeoutParamTypeId).toUInt();
            bool forceBattery = thing->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
            qCWarning(dcSolaxOnly()) << "Battery timer should be set to" << timeout;
            if (forceBattery && timeout > 0) {
                qCWarning(dcSolaxOnly()) << "Battery timer will be set to" << timeout << "as manual mode is enabled";
                if (m_batteryPowerTimer->isActive()) {
                    m_batteryPowerTimer->stop();
                }
                m_batteryPowerTimer->setInterval(timeout*1000);
                m_batteryPowerTimer->start();
            }
            if (timeout > 0) {
                thing->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, timeout);
            }

            int batteryPower = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt(); 
            if (forceBattery)
            {
                setBatteryPower(inverterThing, batteryPower, timeout);
            }
        } else if (action.actionTypeId() == solaxBatteryMaxChargingCurrentActionTypeId) {
            double maxCurrent = action.paramValue(solaxBatteryMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toDouble();
            bool enableMaxCurrent = thing->stateValue(solaxBatteryEnableMaxChargingCurrentStateTypeId).toBool();

            if (maxCurrent != thing->stateValue(solaxBatteryMaxChargingCurrentStateTypeId).toDouble()){
                qCWarning(dcSolaxOnly()) << "Battery max current changed to" << maxCurrent << "A";
                if (enableMaxCurrent && maxCurrent >= 0 && maxCurrent <= 30)
                {
                    thing->setStateValue(solaxBatteryMaxChargingCurrentStateTypeId, maxCurrent);
                    setMaxCurrent(inverterThing, maxCurrent);
                }  
            }          
        } else if (action.actionTypeId() == solaxBatteryEnableMaxChargingCurrentActionTypeId) {
            bool enableMaxCurrent = action.paramValue(solaxBatteryEnableMaxChargingCurrentActionEnableMaxChargingCurrentParamTypeId).toBool();
            double maxCurrent = thing->stateValue(solaxBatteryMaxChargingCurrentStateTypeId).toDouble();
            
            if (enableMaxCurrent != thing->stateValue(solaxBatteryEnableMaxChargingCurrentStateTypeId).toBool()){
                qCWarning(dcSolaxOnly()) << "Enabling of battery max current changed to" << enableMaxCurrent;
                if (!enableMaxCurrent) {
                    setMaxCurrent(inverterThing, 30.0); //reset to default value 30A
                    thing->setStateValue(solaxBatteryMaxChargingCurrentStateTypeId, 30.0);
                } else if (enableMaxCurrent && maxCurrent >= 0 && maxCurrent <= 30)
                {
                    setMaxCurrent(inverterThing, maxCurrent);
                }
                thing->setStateValue(solaxBatteryEnableMaxChargingCurrentStateTypeId, enableMaxCurrent); 
            }                
        } else if (action.actionTypeId() == solaxBatteryMinBatteryLevelActionTypeId) {
            int minBatteryLevel = action.paramValue(solaxBatteryMinBatteryLevelActionMinBatteryLevelParamTypeId).toInt();
            qCWarning(dcSolaxOnly()) << "Min battery level set to" << minBatteryLevel;
            thing->setStateValue(solaxBatteryMinBatteryLevelStateTypeId, minBatteryLevel);
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    }
}

void IntegrationPluginSolax::thingRemoved(Thing *thing)
{
    if (m_tcpConnections.contains(thing)) {
        SolaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (m_iesConnections.contains(thing)) {
        SolaxIesModbusTcpConnection *connection = m_iesConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_meterstates.contains(thing)) {
        m_meterstates.remove(thing);
    }

    if (m_batterystates.contains(thing)) {
        m_batterystates.remove(thing);
    }

    if (m_batteryPowerTimer) {
        if (m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->stop();
        }
        delete m_batteryPowerTimer;
        m_batteryPowerTimer = nullptr;
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSolax::setRunMode(Thing *thing, quint16 runModeAsInt)
{
    QString runModeAsString{"Unknown"};
    switch (runModeAsInt) {
        case 0:
            runModeAsString = "Waiting";
            break;
        case 1:
            runModeAsString = "Checking";
            break;
        case 2:
            runModeAsString = "Normal";
            break;
        case 3:
            runModeAsString = "Fault";
            break;
        case 4:
            runModeAsString = "Permanent fault";
            break;
        case 5:
            runModeAsString = "Updating";
            break;
        case 6:
            runModeAsString = "EPS Check";
            break;
        case 7:
            runModeAsString = "EPS";
            break;
        case 8:
            runModeAsString = "Self Test";
            break;
        case 9:
            runModeAsString = "Idle";
            break;
    }
    if (thing->thingClassId() == solaxX3UltraThingClassId) {
        thing->setStateValue(solaxX3UltraInverterStatusStateTypeId, runModeAsString);
    }
}

void IntegrationPluginSolax::setErrorMessage(Thing *thing, quint32 errorBits)
{
    QString errorMessage{""};
    if (errorBits == 0) {
        errorMessage = "No error, everything ok.";
    } else {
        errorMessage.append("The following error bits are active: ");
        quint32 testBit{0b01};
        for (int var = 0; var < 32; ++var) {
            if (errorBits & (testBit << var)) {
                errorMessage.append(QStringLiteral("bit %1").arg(var));
                if (var == 0) {
                    errorMessage.append(" HardwareTrip, ");
                } else if (var == 1) {
                    errorMessage.append(" MainsLostFault, ");
                } else if (var == 2) {
                    errorMessage.append(" GridVoltFault, ");
                } else if (var == 3) {
                    errorMessage.append(" GridFreqFault, ");
                } else if (var == 4) {
                    errorMessage.append(" PvVoltFault, ");
                } else if (var == 5) {
                    errorMessage.append(" BusVoltFault, ");
                } else if (var == 6) {
                    errorMessage.append(" Bat Volt Fault, ");
                } else if (var == 7) {
                    errorMessage.append(" Ac10Mins_Voltage_Fault, ");
                } else if (var == 8) {
                    errorMessage.append(" Dci_OCP_Fault, ");
                } else if (var == 9) {
                    errorMessage.append(" Dcv_OCP_Fault, ");
                } else if (var == 10) {
                    errorMessage.append(" SW_OCP_Fault, ");
                } else if (var == 11) {
                    errorMessage.append(" RC_OCP_Fault, ");
                } else if (var == 12) {
                    errorMessage.append(" IsolationFault, ");
                } else if (var == 13) {
                    errorMessage.append(" TemperatureOverFault, ");
                } else if (var == 14) {
                    errorMessage.append(" BatConDir_Fault, ");
                } else if (var == 15) {
                    errorMessage.append(" SampleConsistenceFault, ");
                } else if (var == 16) {
                    errorMessage.append(" EpsOverLoad, ");
                } else if (var == 17) {
                    errorMessage.append(" EPS_OCP_Fault, ");
                } else if (var == 18) {
                    errorMessage.append(" InputConfigFault, ");
                } else if (var == 19) {
                    errorMessage.append(" FirmwareVerFault, ");
                } else if (var == 20) {
                    errorMessage.append(" EPSBatPowerLow, ");
                } else if (var == 21) {
                    errorMessage.append(" PhaseAngleFault, ");
                } else if (var == 22) {
                    errorMessage.append(" PLL_OverTime, ");
                } else if (var == 23) {
                    errorMessage.append(" ParallelFault, ");
                } else if (var == 24) {
                    errorMessage.append(" Inter_Com_Fault, ");
                } else if (var == 25) {
                    errorMessage.append(" Fan Fault, ");
                } else if (var == 26) {
                    errorMessage.append(" HCT_AC_DeviceFault, ");
                } else if (var == 27) {
                    errorMessage.append(" EepromFault, ");
                } else if (var == 28) {
                    errorMessage.append(" ResidualCurrent_DeviceFault, ");
                } else if (var == 29) {
                    errorMessage.append(" EpsRelayFault, ");
                } else if (var == 30) {
                    errorMessage.append(" GridRelayFault, ");
                } else if (var == 31) {
                    errorMessage.append(" BatRelayFault, ");
                } else {
                    errorMessage.append(", ");
                }
            }
        }
        int stringLength = errorMessage.length();
        if (stringLength > 1) { // stringLength should be > 1, but just in case.
            errorMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
        }
    }
    qCDebug(dcSolaxOnly()) << errorMessage;
    if (thing->thingClassId() == solaxX3UltraThingClassId) {
        thing->setStateValue(solaxX3UltraErrorMessageStateTypeId, errorMessage);
    }
}

void IntegrationPluginSolax::setBmsWarningMessage(Thing *thing)
{
    QString warningMessage{""};
    quint16 warningBitsLsb = m_batterystates.value(thing).bmsWarningLsb;
    quint16 warningBitsMsb = m_batterystates.value(thing).bmsWarningMsb;

    quint32 warningBits = warningBitsMsb;
    warningBits = (warningBits << 16) + warningBitsLsb;

    if (warningBits == 0) {
        warningMessage = "No warning, everything ok.";
    } else {
        warningMessage.append("The following warning bits are active: ");
        quint32 testBit{0b01};
        for (int var = 0; var < 32; ++var) {
            if (warningBits & (testBit << var)) {
                warningMessage.append(QStringLiteral("bit %1").arg(var));
                if (var == 0) {
                    warningMessage.append(" BMS_External_Err, ");
                } else if (var == 1) {
                    warningMessage.append(" BMS_Internal_Err, ");
                } else if (var == 2) {
                    warningMessage.append(" BMS_OverVoltage, ");
                } else if (var == 3) {
                    warningMessage.append(" BMS_LowerVoltage, ");
                } else if (var == 4) {
                    warningMessage.append(" BMS_ChargeOverCurrent, ");
                } else if (var == 5) {
                    warningMessage.append(" BMS_DishargeOverCurrent, ");
                } else if (var == 6) {
                    warningMessage.append(" BMS_TemHigh, ");
                } else if (var == 7) {
                    warningMessage.append(" BMS_TemLow, ");
                } else if (var == 8) {
                    warningMessage.append(" BMS_CellImblance, ");
                } else if (var == 9) {
                    warningMessage.append(" BMS_Hardware_Prot, ");
                } else if (var == 10) {
                    warningMessage.append(" BMS_Inlock_Fault, ");
                } else if (var == 11) {
                    warningMessage.append(" BMS_ISO_Fault, ");
                } else if (var == 12) {
                    warningMessage.append(" BMS_VolSen_Fault, ");
                } else if (var == 13) {
                    warningMessage.append(" BMS_TempSen_Fault, ");
                } else if (var == 14) {
                    warningMessage.append(" BMS_CurSen_Fault, ");
                } else if (var == 15) {
                    warningMessage.append(" BMS_Relay_Fault, ");
                } else if (var == 16) {
                    warningMessage.append(" BMS_Type_Unmatch, ");
                } else if (var == 17) {
                    warningMessage.append(" BMS_Ver_Unmatch, ");
                } else if (var == 18) {
                    warningMessage.append(" BMS_Manufacturer_Unmatch, ");
                } else if (var == 19) {
                    warningMessage.append(" BMS_SW&HW_Unmatch, ");
                } else if (var == 20) {
                    warningMessage.append(" BMS_M&S_Unmatch, ");
                } else if (var == 21) {
                    warningMessage.append(" BMS_CR_Unresponsive, ");
                } else if (var == 22) {
                    warningMessage.append(" BMS_Software_Protect, ");
                } else if (var == 23) {
                    warningMessage.append(" BMS_536_Fault, ");
                } else if (var == 24) {
                    warningMessage.append(" Selfchecking_Fault, ");
                } else if (var == 25) {
                    warningMessage.append(" BMS_Tempdiff_Fault, ");
                } else if (var == 26) {
                    warningMessage.append(" BMS_Break, ");
                } else if (var == 27) {
                    warningMessage.append(" BMS_Flash_Fault, ");
                } else {
                    warningMessage.append(", ");
                }
            }
        }
        int stringLength = warningMessage.length();
        if (stringLength > 1) { // stringLength should be > 1, but just in case.
            warningMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
        }
    }
    qCDebug(dcSolaxOnly()) << warningMessage;
    thing->setStateValue("warningMessage", warningMessage);
    qCDebug(dcSolaxOnly()) << "Setting BMS Warning Message successfull";
}

void IntegrationPluginSolax::setBatteryPower(Thing *thing, qint32 powerToSet, quint16 batteryTimeout)
{
    if (thing->thingClassId() == solaxIESInverterTCPThingClassId) {
        SolaxIesModbusTcpConnection *connection = m_iesConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolaxOnly()) << "setBatteryPower - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->setInterval(batteryTimeout*1000);
            m_batteryPowerTimer->start();
        }

        quint32 modeTypeValue = 4;
        QModbusReply *replyMode = connection->setModeType(modeTypeValue);
        connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
        connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode, connection, powerToSet](){
            if (replyMode->error() != QModbusDevice::NoError) {
                qCWarning(dcSolaxOnly()) << "setBatteryPower - Error setting mode and type" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolaxOnly()) << "setBatteryPower - Mode set to 8, Type set to 1";
                //info->finish(Thing::ThingErrorNoError);
                QModbusReply *replyBatterPower = connection->setForceBatteryPower(-1*powerToSet);
                connect(replyBatterPower, &QModbusReply::finished, replyBatterPower, &QModbusReply::deleteLater);
                connect(replyBatterPower, &QModbusReply::finished, thing, [thing, replyBatterPower, powerToSet](){
                    if (replyBatterPower->error() != QModbusDevice::NoError) {
                        qCWarning(dcSolaxOnly()) << "setBatteryPower - Error setting battery power" << replyBatterPower->error() << replyBatterPower->errorString();
                        //info->finish(Thing::ThingErrorHardwareFailure);
                    } else {
                        qCWarning(dcSolaxOnly()) << "setBatteryPower - Set battery power to" << powerToSet;
                        //info->finish(Thing::ThingErrorNoError);
                    }
                });
            }
        });
    } else {
        qCWarning(dcSolaxOnly()) << "setBatteryPower - Received incorrect thing";
    }
}

void IntegrationPluginSolax::disableRemoteControl(Thing *thing)
{
    if (thing->thingClassId() == solaxIESInverterTCPThingClassId) {
        SolaxIesModbusTcpConnection *connection = m_iesConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolaxOnly()) << "disableRemoteControl - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->stop();
        }

        QModbusReply *replyMode = connection->setModeType(0);
        connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
        connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode](){
            if (replyMode->error() != QModbusDevice::NoError) {
                qCWarning(dcSolaxOnly()) << "disableRemoteControl - Error setting mode and type" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolaxOnly()) << "disableRemoteControl - Mode set to 0";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
        Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
        if (!batteryThings.isEmpty()) {
            uint setTimeout = batteryThings.first()->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
            batteryThings.first()->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, setTimeout);
        }
    } else {
        qCWarning(dcSolaxOnly()) << "disableRemoteControl - Received wrong thing";
    }
    
}

void IntegrationPluginSolax::setMaxCurrent(Thing *thing, double maxCurrent)
{
    if (thing->thingClassId() == solaxIESInverterTCPThingClassId) {
        SolaxIesModbusTcpConnection *connection = m_iesConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolaxOnly()) << "setMaxCurrent - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        QModbusReply *reply = connection->setWriteChargeMaxCurrent(float(maxCurrent));
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, thing, [thing, reply](){
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcSolaxOnly()) << "setMaxChargingCurrent: Error during setting" << reply->error() << reply->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolaxOnly()) << "setMaxChargingCurrent: set successfully ";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
    } else {
        qCWarning(dcSolaxOnly()) << "setBatteryPower - Received incorrect thing";
    }
}

void IntegrationPluginSolax::writeErrorLog()
{
    qCWarning(dcSolaxOnly()) << "WriteErrorLog called";
    // Write to file /data/solax-counter.txt
    QFile errorCounterFile("/data/solax-counter.txt");

    int init = 0;
    int cont = 0;
    errorCounterFile.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
    while (!errorCounterFile.atEnd()) {
        QByteArray line = errorCounterFile.readLine();
        if (line.startsWith("Init")) {
            QList<QByteArray> list = line.split(' ');
            init = list[1].toInt();
        } else {
            QList<QByteArray> list = line.split(' ');
            cont = list[1].toInt();
        }
    }

    if (m_energyCheck == 500) {
        init += 1;
    } else {
        cont += 1;
    }
    errorCounterFile.write("Init: " + QByteArray::number(init) + QByteArray("\nCont: ") + QByteArray::number(cont));
    errorCounterFile.close();
}
