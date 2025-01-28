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

#include "integrationpluginalphainnotec.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginAlphaInnotec::IntegrationPluginAlphaInnotec()
{

}

void IntegrationPluginAlphaInnotec::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcAlphaInnotec()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }


    if (info->thingClassId() == alphaConnectThingClassId) {
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){

            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

                qCDebug(dcAlphaInnotec()) << "Found" << networkDeviceInfo;

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

                ThingDescriptor descriptor(alphaConnectThingClassId, title, description);

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(alphaConnectThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcAlphaInnotec()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(alphaConnectThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
                params << Param(alphaConnectThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });
    }
    if (info->thingClassId() == aitSmartHomeThingClassId) {
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){

            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

                qCDebug(dcAlphaInnotec()) << "Found" << networkDeviceInfo;

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

                ThingDescriptor descriptor(aitSmartHomeThingClassId, title, description);
                ParamList params;
                params << Param(aitSmartHomeThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
                params << Param(aitSmartHomeThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(aitSmartHomeThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcAlphaInnotec()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });
    }
}

void IntegrationPluginAlphaInnotec::startMonitoringAutoThings()
{

}

void IntegrationPluginAlphaInnotec::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAlphaInnotec()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == alphaConnectThingClassId) {
        QHostAddress hostAddress = QHostAddress(thing->paramValue(alphaConnectThingIpAddressParamTypeId).toString());
        if (hostAddress.isNull()) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("No IP address given."));
            return;
        }

        uint port = thing->paramValue(alphaConnectThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(alphaConnectThingSlaveIdParamTypeId).toUInt();

        AlphaInnotecModbusTcpConnection *alphaConnectTcpConnection = new AlphaInnotecModbusTcpConnection(hostAddress, port, slaveId, this);
        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::connectionStateChanged, this, [thing, alphaConnectTcpConnection](bool status){
            qCDebug(dcAlphaInnotec()) << "Connected changed to" << status << "for" << thing;
            if (status) {
                alphaConnectTcpConnection->update();
            }

            thing->setStateValue(alphaConnectConnectedStateTypeId, status);
        });


        // Input registers
//        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::meanTemperatureChanged, this, [thing](float meanTemperature){
//            qCDebug(dcAlphaInnotec()) << thing << "mean temperature changed" << meanTemperature << "°C";
//            thing->setStateValue(alphaConnectMeanTemperatureStateTypeId, meanTemperature);
//        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::flowTemperatureChanged, this, [thing](float flowTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "flow temperature changed" << flowTemperature << "°C";
            thing->setStateValue(alphaConnectFlowTemperatureStateTypeId, flowTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::returnTemperatureChanged, this, [thing](float returnTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "return temperature changed" << returnTemperature << "°C";
            thing->setStateValue(alphaConnectReturnTemperatureStateTypeId, returnTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::externalReturnTemperatureChanged, this, [thing](float externalReturnTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "external return temperature changed" << externalReturnTemperature << "°C";
            thing->setStateValue(alphaConnectExternalReturnTemperatureStateTypeId, externalReturnTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::hotWaterTemperatureChanged, this, [thing](float hotWaterTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
            thing->setStateValue(alphaConnectHotWaterTemperatureStateTypeId, hotWaterTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::hotGasTemperatureChanged, this, [thing](float hotGasTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "hot gas temperature changed" << hotGasTemperature << "°C";
            thing->setStateValue(alphaConnectHotGasTemperatureStateTypeId, hotGasTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::heatSourceInletTemperatureChanged, this, [thing](float heatSourceInletTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "heat source inlet temperature changed" << heatSourceInletTemperature << "°C";
            thing->setStateValue(alphaConnectHeatSourceInletTemperatureStateTypeId, heatSourceInletTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::heatSourceOutletTemperatureChanged, this, [thing](float heatSourceOutletTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "heat source outlet temperature changed" << heatSourceOutletTemperature << "°C";
            thing->setStateValue(alphaConnectHeatSourceOutletTemperatureStateTypeId, heatSourceOutletTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::roomTemperature1Changed, this, [thing](float roomTemperature1){
            qCDebug(dcAlphaInnotec()) << thing << "room remote adjuster 1 temperature changed" << roomTemperature1 << "°C";
            thing->setStateValue(alphaConnectRoomTemperature1StateTypeId, roomTemperature1);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::roomTemperature2Changed, this, [thing](float roomTemperature2){
            qCDebug(dcAlphaInnotec()) << thing << "room remote adjuster 2 temperature changed" << roomTemperature2 << "°C";
            thing->setStateValue(alphaConnectRoomTemperature2StateTypeId, roomTemperature2);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::roomTemperature3Changed, this, [thing](float roomTemperature3){
            qCDebug(dcAlphaInnotec()) << thing << "room remote adjuster 3 temperature changed" << roomTemperature3 << "°C";
            thing->setStateValue(alphaConnectRoomTemperature2StateTypeId, roomTemperature3);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::solarCollectorTemperatureChanged, this, [thing](float solarCollectorTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "solar collector temperature changed" << solarCollectorTemperature << "°C";
            thing->setStateValue(alphaConnectSolarCollectorTemperatureStateTypeId, solarCollectorTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::solarStorageTankTemperatureChanged, this, [thing](float solarStorageTankTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "solar storage tank temperature changed" << solarStorageTankTemperature << "°C";
            thing->setStateValue(alphaConnectSolarCollectorTemperatureStateTypeId, solarStorageTankTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::externalEnergySourceTemperatureChanged, this, [thing](float externalEnergySourceTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "external energy source temperature changed" << externalEnergySourceTemperature << "°C";
            thing->setStateValue(alphaConnectExternalEnergySourceTemperatureStateTypeId, externalEnergySourceTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::supplyAirTemperatureChanged, this, [thing](float supplyAirTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "supply air temperature changed" << supplyAirTemperature << "°C";
            thing->setStateValue(alphaConnectSupplyAirTemperatureStateTypeId, supplyAirTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::externalAirTemperatureChanged, this, [thing](float externalAirTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "external air temperature changed" << externalAirTemperature << "°C";
            thing->setStateValue(alphaConnectExternalAirTemperatureStateTypeId, externalAirTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::heatingPumpOperatingHoursChanged, this, [thing](quint16 heatingPumpOperatingHours){
            qCDebug(dcAlphaInnotec()) << thing << "heating pump operating hours changed" << heatingPumpOperatingHours;
            thing->setStateValue(alphaConnectHeatingPumpOperatingHoursStateTypeId, heatingPumpOperatingHours);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::systemStatusChanged, this, [thing](AlphaInnotecModbusTcpConnection::SystemStatus systemStatus){
            qCDebug(dcAlphaInnotec()) << thing << "system status changed" << systemStatus;
            switch (systemStatus) {
            case AlphaInnotecModbusTcpConnection::SystemStatusHeatingMode:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "Heating mode");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusDomesticHotWater:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "Domestic hot water");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusSwimmingPool:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "Swimming pool");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusEVUOff:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "EUV off");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusDefrost:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "Defrost");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusOff:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "Off");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusExternalEnergySource:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "External energy source");
                break;
            case AlphaInnotecModbusTcpConnection::SystemStatusCoolingMode:
                thing->setStateValue(alphaConnectSystemStatusStateTypeId, "Cooling mode");
                break;
            }

            // Set heating and cooling states according to the system state
            thing->setStateValue(alphaConnectHeatingOnStateTypeId, systemStatus == AlphaInnotecModbusTcpConnection::SystemStatusHeatingMode);
            thing->setStateValue(alphaConnectCoolingOnStateTypeId, systemStatus == AlphaInnotecModbusTcpConnection::SystemStatusCoolingMode);
        });

        // Energy
        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::totalHeatEnergyChanged, this, [thing](float totalHeatEnergy){
            qCDebug(dcAlphaInnotec()) << thing << "total heating energy changed" << totalHeatEnergy << "kWh";
            thing->setStateValue(alphaConnectTotalEnergyStateTypeId, totalHeatEnergy);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::heatingEnergyChanged, this, [thing](float heatingEnergy){
            qCDebug(dcAlphaInnotec()) << thing << "heating energy changed" << heatingEnergy << "kWh";
            thing->setStateValue(alphaConnectHeatingEnergyStateTypeId, heatingEnergy);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::waterHeatEnergyChanged, this, [thing](float waterHeatEnergy){
            qCDebug(dcAlphaInnotec()) << thing << "water heat energy changed" << waterHeatEnergy << "kWh";
            thing->setStateValue(alphaConnectHotWaterEnergyStateTypeId, waterHeatEnergy);
        });

//        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::swimmingPoolHeatEnergyChanged, this, [thing](float swimmingPoolHeatEnergy){
//            qCDebug(dcAlphaInnotec()) << thing << "swimming pool heat energy changed" << swimmingPoolHeatEnergy << "kWh";
//            thing->setStateValue(alphaConnectSwimmingPoolEnergyStateTypeId, swimmingPoolHeatEnergy);
//        });

        // Holding registers
        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::outdoorTemperatureChanged, this, [thing](float outdoorTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
            thing->setStateValue(alphaConnectOutdoorTemperatureStateTypeId, outdoorTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::returnSetpointTemperatureChanged, this, [thing](float returnSetpointTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "return setpoint temperature changed" << returnSetpointTemperature << "°C";
            thing->setStateValue(alphaConnectReturnSetpointTemperatureStateTypeId, returnSetpointTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::hotWaterSetpointTemperatureChanged, this, [thing](float hotWaterSetpointTemperature){
            qCDebug(dcAlphaInnotec()) << thing << "hot water setpoint temperature changed" << hotWaterSetpointTemperature << "°C";
            thing->setStateValue(alphaConnectHotWaterSetpointTemperatureStateTypeId, hotWaterSetpointTemperature);
        });

        connect(alphaConnectTcpConnection, &AlphaInnotecModbusTcpConnection::smartGridChanged, this, [thing](AlphaInnotecModbusTcpConnection::SmartGridState smartGridState){
            qCDebug(dcAlphaInnotec()) << thing << "smart grid state changed" << smartGridState;
            switch (smartGridState) {
            case AlphaInnotecModbusTcpConnection::SmartGridStateOff:
                thing->setStateValue(alphaConnectSgReadyModeStateTypeId, "Off");
                break;
            case AlphaInnotecModbusTcpConnection::SmartGridStateLow:
                thing->setStateValue(alphaConnectSgReadyModeStateTypeId, "Low");
                break;
            case AlphaInnotecModbusTcpConnection::SmartGridStateStandard:
                thing->setStateValue(alphaConnectSgReadyModeStateTypeId, "Standard");
                break;
            case AlphaInnotecModbusTcpConnection::SmartGridStateHigh:
                thing->setStateValue(alphaConnectSgReadyModeStateTypeId, "High");
                break;
            }
        });

        m_connections.insert(thing, alphaConnectTcpConnection);
        alphaConnectTcpConnection->connectDevice();

        // FIXME: make async and check if this is really an alpha connect
        info->finish(Thing::ThingErrorNoError);
    }

    if (thing->thingClassId() == aitSmartHomeThingClassId) {

        // Handle reconfigure
        if (m_aitShiConnections.contains(thing)) {
            qCDebug(dcAlphaInnotec()) << "Reconfiguring existing thing" << thing->name();
            m_aitShiConnections.take(thing)->deleteLater();
            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }

        MacAddress macAddress = MacAddress(thing->paramValue(aitSmartHomeThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcAlphaInnotec()) << "The configured MAC address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the heatpump."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Clean up in case the setup gets aborted
            if (m_monitors.contains(thing)) {
                qCDebug(dcAlphaInnotec()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();
        uint port = thing->paramValue(aitSmartHomeThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(aitSmartHomeThingSlaveIdParamTypeId).toUInt();

        qCInfo(dcAlphaInnotec()) << "Setting up AlphaInnotec on" << address.toString();
        auto aitShiConnection = new aitShiModbusTcpConnection(address, port , slaveId, this);
        connect(info, &ThingSetupInfo::aborted, aitShiConnection, &aitShiModbusTcpConnection::deleteLater);

        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcAlphaInnotec()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->setupComplete())
                return;

            if (reachable && !thing->stateValue("connected").toBool()) {
                aitShiConnection->setHostAddress(monitor->networkDeviceInfo().address());
                aitShiConnection->reconnectDevice();
            } else if (!reachable) {
                // Note: Auto reconnect is disabled explicitly and
                // the device will be connected once the monitor says it is reachable again
                aitShiConnection->disconnectDevice();
            }
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::reachableChanged, thing, [this, thing, aitShiConnection](bool reachable){
            qCInfo(dcAlphaInnotec()) << "Reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                aitShiConnection->initialize();
            } else {
                thing->setStateValue("connected", false);
            }
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue("connected", success);

            if (!success) {
                // Try once to reconnect the device
                aitShiConnection->reconnectDevice();
            } else {
                qCInfo(dcAlphaInnotec()) << "Connection initialized successfully for" << thing;
                writeOffsetTemperatures(thing);
                aitShiConnection->update();
            }
        });
        
        // TODO: Connect the states here
        connect(aitShiConnection, &aitShiModbusTcpConnection::returnTempCurrentChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Current return temperature changed to" << temperature;
            thing->setStateValue(aitSmartHomeReturnTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::returnTempTargetChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Return temperature target changed to" << temperature;
            thing->setStateValue(aitSmartHomeReturnSetpointTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::flowTemperatureChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Flow temperature target changed to" << temperature;
            thing->setStateValue(aitSmartHomeFlowTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::roomTemperatureChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Romm temperature changed" << temperature;
            thing->setStateValue(aitSmartHomeRoomTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::outdoorTemperatureChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Outdoor temperature changed" << temperature;
            thing->setStateValue(aitSmartHomeOutdoorTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::hotWaterTempCurrentChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Hot water temperature changed" << temperature;
            thing->setStateValue(aitSmartHomeHotWaterTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::hotWaterTempTargetChanged, thing, [thing](float temperature) {
            qCDebug(dcAlphaInnotec()) << "Hot water setpoint temperature changed" << temperature;
            thing->setStateValue(aitSmartHomeHotWaterSetpointTemperatureStateTypeId, temperature);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::errorCodeChanged, thing, [thing](quint16 code) {
            qCDebug(dcAlphaInnotec()) << "Error code changed to" << code;
            thing->setStateValue(aitSmartHomeErrorCodeStateTypeId, code);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::electricalPowerChanged, thing, [thing](float power) {
            qCDebug(dcAlphaInnotec()) << "Current electrical power changed to" << power;
            thing->setStateValue(aitSmartHomeCurrentPowerStateTypeId, power);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::heatingPowerChanged, thing, [thing](float power) {
            qCDebug(dcAlphaInnotec()) << "Current heating power changed to" << power;
            thing->setStateValue(aitSmartHomeHeatingPowerStateTypeId, power);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::totalEnergyConsumedChanged, thing, [thing](float energy) {
            qCDebug(dcAlphaInnotec()) << "Total energy consumed changed to" << energy;
            thing->setStateValue(aitSmartHomeTotalEnergyConsumedStateTypeId, energy);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::operatingStateChanged, thing, [thing](aitShiModbusTcpConnection::SystemStatus systemStatus){
            qCDebug(dcAlphaInnotec()) << thing << "system status changed" << systemStatus;
            switch (systemStatus) {
            case aitShiModbusTcpConnection::SystemStatusHeatingMode:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "Heating mode");
                break;
            case aitShiModbusTcpConnection::SystemStatusDomesticHotWater:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "Domestic hot water");
                break;
            case aitShiModbusTcpConnection::SystemStatusSwimmingPool:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "Swimming pool");
                break;
            case aitShiModbusTcpConnection::SystemStatusEVUOff:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "EUV off");
                break;
            case aitShiModbusTcpConnection::SystemStatusDefrost:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "Defrost");
                break;
            case aitShiModbusTcpConnection::SystemStatusOff:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "Off");
                break;
            case aitShiModbusTcpConnection::SystemStatusExternalEnergySource:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "External energy source");
                break;
            case aitShiModbusTcpConnection::SystemStatusCoolingMode:
                thing->setStateValue(aitSmartHomeSystemStatusStateTypeId, "Cooling mode");
                break;
            }

            // Set heating and cooling states according to the system state
            thing->setStateValue(aitSmartHomeHeatingOnStateTypeId, systemStatus == aitShiModbusTcpConnection::SystemStatusHeatingMode);
            thing->setStateValue(aitSmartHomeCoolingOnStateTypeId, systemStatus == aitShiModbusTcpConnection::SystemStatusCoolingMode);
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::softwarePlatformChanged, thing, [this, thing](quint16 version) {
            qCDebug(dcAlphaInnotec()) << "Software Platform changed to" << version;
            updateFirmwareVersion(thing, version, "platform");
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::majorVersionChanged, thing, [this, thing](quint16 version) {
            qCDebug(dcAlphaInnotec()) << "Major version changed to" << version;
            updateFirmwareVersion(thing, version, "major");
        });

        connect(aitShiConnection, &aitShiModbusTcpConnection::minorVersionChanged, thing, [this, thing](quint16 version) {
            qCDebug(dcAlphaInnotec()) << "Minor version changed to" << version;
            updateFirmwareVersion(thing, version, "minor");
        });

        m_aitShiConnections.insert(thing, aitShiConnection);

        if (monitor->reachable())
            aitShiConnection->connectDevice();

        info->finish(Thing::ThingErrorNoError);
        return;
    }
}

void IntegrationPluginAlphaInnotec::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == alphaConnectThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcAlphaInnotec()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach (AlphaInnotecModbusTcpConnection *connection, m_connections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }

                foreach (aitShiModbusTcpConnection *connection, m_aitShiConnections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginAlphaInnotec::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == alphaConnectThingClassId && m_connections.contains(thing)) {
        AlphaInnotecModbusTcpConnection *connection = m_connections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (thing->thingClassId() == aitSmartHomeThingClassId && m_aitShiConnections.contains(thing)) {
        aitShiModbusTcpConnection *connection = m_aitShiConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginAlphaInnotec::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == alphaConnectThingClassId) {
        AlphaInnotecModbusTcpConnection *connection = m_connections.value(thing);

        if (!connection->connected()) {
            qCWarning(dcAlphaInnotec()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

  /*      if (info->action().actionTypeId() == alphaConnectOutdoorTemperatureActionTypeId) {
            double outdoorTemperature = info->action().paramValue(alphaConnectOutdoorTemperatureActionOutdoorTemperatureParamTypeId).toDouble();
            qCDebug(dcAlphaInnotec()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
            QModbusReply *reply = connection->setOutdoorTemperature(outdoorTemperature);
            if (!reply) {
                qCWarning(dcAlphaInnotec()) << "Execute action failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, outdoorTemperature]{
                if (reply->error() != QModbusDevice::NoError) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    qCWarning(dcAlphaInnotec()) << "Set outdoor temperature finished with error" << reply->errorString();
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->thing()->setStateValue(alphaConnectOutdoorTemperatureStateTypeId, outdoorTemperature);
                info->finish(Thing::ThingErrorNoError);
            });

            connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            });
        } else */
        if (info->action().actionTypeId() == alphaConnectHotWaterSetpointTemperatureActionTypeId) {
            double temperature = info->action().paramValue(alphaConnectHotWaterSetpointTemperatureActionHotWaterSetpointTemperatureParamTypeId).toDouble();
            qCDebug(dcAlphaInnotec()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
            QModbusReply *reply = connection->setHotWaterSetpointTemperature(temperature);
            if (!reply) {
                qCWarning(dcAlphaInnotec()) << "Execute action failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, temperature]{
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set hot water setpoint temperature finished with error" << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->thing()->setStateValue(alphaConnectHotWaterSetpointTemperatureStateTypeId, temperature);
                info->finish(Thing::ThingErrorNoError);
            });

            connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            });
        } else if (info->action().actionTypeId() == alphaConnectReturnSetpointTemperatureActionTypeId) {
            double temperature = info->action().paramValue(alphaConnectReturnSetpointTemperatureActionReturnSetpointTemperatureParamTypeId).toDouble();
            qCDebug(dcAlphaInnotec()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
            QModbusReply *reply = connection->setReturnSetpointTemperature(temperature);
            if (!reply) {
                qCWarning(dcAlphaInnotec()) << "Execute action failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, temperature]{
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set return setpoint temperature finished with error" << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->thing()->setStateValue(alphaConnectReturnSetpointTemperatureStateTypeId, temperature);
                info->finish(Thing::ThingErrorNoError);
            });

            connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            });
        } else if (info->action().actionTypeId() == alphaConnectSgReadyModeActionTypeId) {
            QString sgReadyModeString = info->action().paramValue(alphaConnectSgReadyModeActionSgReadyModeParamTypeId).toString();
            qCDebug(dcAlphaInnotec()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
            AlphaInnotecModbusTcpConnection::SmartGridState sgReadyState;
            if (sgReadyModeString == "Off") {
                sgReadyState = AlphaInnotecModbusTcpConnection::SmartGridStateOff;
            } else if (sgReadyModeString == "Low") {
                sgReadyState = AlphaInnotecModbusTcpConnection::SmartGridStateLow;
            } else if (sgReadyModeString == "High") {
                sgReadyState = AlphaInnotecModbusTcpConnection::SmartGridStateHigh;
            } else {
                sgReadyState = AlphaInnotecModbusTcpConnection::SmartGridStateStandard;
            }

            QModbusReply *reply = connection->setSmartGrid(sgReadyState);
            if (!reply) {
                qCWarning(dcAlphaInnotec()) << "Execute action failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyModeString]{
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set SG ready mode finished with error" << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->thing()->setStateValue(alphaConnectSgReadyModeStateTypeId, sgReadyModeString);
                info->finish(Thing::ThingErrorNoError);
            });

            connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            });
        }
    }

    if (thing->thingClassId() == alphaConnectThingClassId) {
        aitShiModbusTcpConnection *connection = m_aitShiConnections.value(thing);

        if (!connection->connected()) {
            qCWarning(dcAlphaInnotec()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (info->action().actionTypeId() == aitSmartHomeActualPvSurplusActionTypeId)
        {
            qCDebug(dcAlphaInnotec()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
            double surplusPvPower = info->action().paramValue(aitSmartHomeActualPvSurplusActionActualPvSurplusParamTypeId).toDouble();
            /* TODO: Plan is the following:
             * CHECK if this assumption is correct: 
             * Create two function to set hotwater and heating temperature at parameter change / setup, as it does not need to be done every time
             *
             * IF surplusPvPower == 0 THEN:
             *  SET pcLimit TO surplusPvPower
             *  SET lpcMode TO 2
             *  SET hotWaterMode TO 0
             *  SET heatingMode TO 0
             *
             * ELSE IF suplusPvPower > 0 THEN:
             *  SET pcLimit TO surplusPvPower
             *  SET lpcMode TO 1
             *  SET hotWaterMode TO 2
             *  SET heatingMode TO 2
             */

            bool surplusIsZero = (surplusPvPower == 0) ? true : false;

            // Set PC Limit
            QModbusReply *pcLimitReply = connection->setPcLimit(surplusPvPower);
            if (!pcLimitReply) {
                qCWarning(dcAlphaInnotec()) << "Execute action setPcLimit failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(pcLimitReply, &QModbusReply::finished, pcLimitReply, &QModbusReply::deleteLater);
            connect(pcLimitReply, &QModbusReply::finished, info, [info, pcLimitReply,surplusPvPower, connection]{
                if (pcLimitReply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set pc limit finished with error" << pcLimitReply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action setPcLimit finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->thing()->setStateValue(aitSmartHomeActualPvSurplusStateTypeId, surplusPvPower);
                info->finish(Thing::ThingErrorNoError);
            });

            connect(pcLimitReply, &QModbusReply::errorOccurred, this, [pcLimitReply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action setPcLimit" << error << pcLimitReply->errorString();
            });

            // Set LPC Mode
            QModbusReply *lpcModeReply = connection->setLpcMode(surplusIsZero ? 2 : 1);
            if (!lpcModeReply) {
                qCWarning(dcAlphaInnotec()) << "Execute action setLpcMode failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(lpcModeReply, &QModbusReply::finished, lpcModeReply, &QModbusReply::deleteLater);
            connect(lpcModeReply, &QModbusReply::finished, info, [info, lpcModeReply, connection]{
                if (lpcModeReply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set pc limit finished with error" << lpcModeReply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->finish(Thing::ThingErrorNoError);
            });

            connect(lpcModeReply, &QModbusReply::errorOccurred, this, [lpcModeReply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << lpcModeReply->errorString();
            });

            // Set Hot Water Mode
            QModbusReply *hotWaterModeReply = connection->setHotWaterMode(surplusIsZero ? 0 : 2);
            if (!hotWaterModeReply) {
                qCWarning(dcAlphaInnotec()) << "Execute action setLpcMode failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(hotWaterModeReply, &QModbusReply::finished, hotWaterModeReply, &QModbusReply::deleteLater);
            connect(hotWaterModeReply, &QModbusReply::finished, info, [info, hotWaterModeReply, connection]{
                if (hotWaterModeReply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set pc limit finished with error" << hotWaterModeReply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->finish(Thing::ThingErrorNoError);
            });

            connect(hotWaterModeReply, &QModbusReply::errorOccurred, this, [hotWaterModeReply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << hotWaterModeReply->errorString();
            });

            // Set Heating Mode
            QModbusReply *heatingModeReply = connection->setHeatingMode(surplusIsZero ? 0 : 2);
            if (!heatingModeReply) {
                qCWarning(dcAlphaInnotec()) << "Execute action setLpcMode failed because the reply could not be created.";
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(heatingModeReply, &QModbusReply::finished, heatingModeReply, &QModbusReply::deleteLater);
            connect(heatingModeReply, &QModbusReply::finished, info, [info, heatingModeReply, connection]{
                if (heatingModeReply->error() != QModbusDevice::NoError) {
                    qCWarning(dcAlphaInnotec()) << "Set pc limit finished with error" << heatingModeReply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }

                qCDebug(dcAlphaInnotec()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
                info->finish(Thing::ThingErrorNoError);
            });

            connect(heatingModeReply, &QModbusReply::errorOccurred, this, [heatingModeReply] (QModbusDevice::Error error){
                qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while execute action" << error << heatingModeReply->errorString();
            });
        }
    }

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginAlphaInnotec::updateFirmwareVersion(Thing *thing, quint16 version, QString place)
{
    if (place == "platform") {
        m_platformVersion = version;
    } else if (place == "major") {
        m_majorVersion = version;
    } else if (place == "minor") {
        m_minorVersion = version;
    }
    QString fwVersion = QString::number(m_platformVersion) + "." + QString::number(m_majorVersion) + "." + QString::number(m_minorVersion);
    thing->setStateValue(aitSmartHomeFirmwareVersionStateTypeId, fwVersion);
}

void IntegrationPluginAlphaInnotec::writeOffsetTemperatures(Thing *thing)
{
    qCDebug(dcAlphaInnotec()) << "Writing offset temperature to heat pump";

    aitShiModbusTcpConnection *connection = m_aitShiConnections.value(thing);

    if (!connection->connected()) {
        qCWarning(dcAlphaInnotec()) << "Could not execute action. The modbus connection is currently not available.";
        return;
    }

    // Hot water offset temperature
    double hotWaterTemp = thing->paramValue(aitSmartHomeThingHotWaterOffsetParamTypeId).toUInt();
    QModbusReply *hotWaterOffsetReply = connection->setHotWaterOffsetTemp(hotWaterTemp);
    if (!hotWaterOffsetReply) {
        qCWarning(dcAlphaInnotec()) << "setHotWaterOffsetTemp failed because the reply could not be created.";
        writeOffsetTemperatures(thing);
        return;
    }

    connect(hotWaterOffsetReply, &QModbusReply::finished, hotWaterOffsetReply, &QModbusReply::deleteLater);
    connect(hotWaterOffsetReply, &QModbusReply::finished, this, [this, thing, hotWaterOffsetReply]{
        if (hotWaterOffsetReply->error() != QModbusDevice::NoError) {
            qCWarning(dcAlphaInnotec()) << "Set hot water offset temp finished with error" << hotWaterOffsetReply->errorString();
            writeOffsetTemperatures(thing);
            return;
        }
    });

    connect(hotWaterOffsetReply, &QModbusReply::errorOccurred, this, [this, thing, hotWaterOffsetReply] (QModbusDevice::Error error){
        qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while executing setHotWaterOffseTemp" << error << hotWaterOffsetReply->errorString();
        writeOffsetTemperatures(thing);
        return;
    });

    // Heating offset temperature
    double heatingTemp = thing->paramValue(aitSmartHomeThingHeatingOffsetParamTypeId).toUInt();
    QModbusReply *heatingOffsetReply = connection->setHeatingOffsetTemp(heatingTemp);
    if (!heatingOffsetReply) {
        qCWarning(dcAlphaInnotec()) << "setHeatingOffsetTemp failed because the reply could not be created.";
        writeOffsetTemperatures(thing);
        return;
    }

    connect(heatingOffsetReply, &QModbusReply::finished, heatingOffsetReply, &QModbusReply::deleteLater);
    connect(heatingOffsetReply, &QModbusReply::finished, this, [this, thing, heatingOffsetReply]{
        if (heatingOffsetReply->error() != QModbusDevice::NoError) {
            qCWarning(dcAlphaInnotec()) << "Set heating offset temp finished with error" << heatingOffsetReply->errorString();
            return;
        }
    });

    connect(heatingOffsetReply, &QModbusReply::errorOccurred, this, [this, thing, heatingOffsetReply] (QModbusDevice::Error error){
        qCWarning(dcAlphaInnotec()) << "Modbus reply error occurred while executing setHeatingOffsetTemp" << error << heatingOffsetReply->errorString();
    });
}
