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

#include "integrationpluginlambda.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginLambda::IntegrationPluginLambda()
{

}

void IntegrationPluginLambda::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcLambda()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){

        foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

            qCDebug(dcLambda()) << "Found" << networkDeviceInfo;

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

            ThingDescriptor descriptor(lambdaTCPThingClassId, title, description);
            ParamList params;
            params << Param(lambdaTCPThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
            params << Param(lambdaTCPThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
            descriptor.setParams(params);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(lambdaTCPThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcLambda()) << "This connection already exists in the system:" << networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    });
}

void IntegrationPluginLambda::startMonitoringAutoThings()
{

}

void IntegrationPluginLambda::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcLambda()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == lambdaTCPThingClassId) {
        QHostAddress hostAddress = QHostAddress(thing->paramValue(lambdaTCPThingIpAddressParamTypeId).toString());
        if (hostAddress.isNull()) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("No IP address given"));
            return;
        }

        quint16 port = thing->paramValue(lambdaTCPThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(lambdaTCPThingSlaveIdParamTypeId).toUInt();

        LambdaModbusTcpConnection *lambdaTCPTcpConnection = new LambdaModbusTcpConnection(hostAddress, port, slaveId, this);
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::connectionStateChanged, this, [thing, lambdaTCPTcpConnection](bool status){
            qCDebug(dcLambda()) << "Connected changed to" << status << "for" << thing;
            if (status) {
                lambdaTCPTcpConnection->update();
            }

            thing->setStateValue(lambdaTCPConnectedStateTypeId, status);
        });


        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::flowTemperatureChanged, thing, [thing](float flowTemperature){
            qCDebug(dcLambda()) << thing << "flow temperature changed" << flowTemperature << "°C";
            thing->setStateValue(lambdaTCPFlowTemperatureStateTypeId, flowTemperature);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::returnTemperatureChanged, thing, [thing](float returnTemperature){
            qCDebug(dcLambda()) << thing << "return temperature changed" << returnTemperature << "°C";
            thing->setStateValue(lambdaTCPReturnTemperatureStateTypeId, returnTemperature);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::hotWaterTemperatureChanged, thing, [thing](float hotWaterTemperature){
            qCDebug(dcLambda()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
            thing->setStateValue(lambdaTCPHotWaterTemperatureStateTypeId, hotWaterTemperature);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatSourceInletTemperatureChanged, thing, [thing](float heatSourceInletTemperature){
            qCDebug(dcLambda()) << thing << "heat source inlet temperature changed" << heatSourceInletTemperature << "°C";
            thing->setStateValue(lambdaTCPHeatSourceInletTemperatureStateTypeId, heatSourceInletTemperature);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatSourceOutletTemperatureChanged, thing, [thing](float heatSourceOutletTemperature){
            qCDebug(dcLambda()) << thing << "heat source outlet temperature changed" << heatSourceOutletTemperature << "°C";
            thing->setStateValue(lambdaTCPHeatSourceOutletTemperatureStateTypeId, heatSourceOutletTemperature);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::roomTemperatureChanged, thing, [thing](float roomTemperature){
            qCDebug(dcLambda()) << thing << "room remote adjuster temperature changed" << roomTemperature << "°C";
            thing->setStateValue(lambdaTCPRoomTemperatureStateTypeId, roomTemperature);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::systemStatusChanged, thing, [thing](LambdaModbusTcpConnection::SystemStatus systemStatus){
            qCDebug(dcLambda()) << thing << "system status changed" << systemStatus;
            switch (systemStatus) {
            case LambdaModbusTcpConnection::SystemStatusInit:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Init");
                break;
            case LambdaModbusTcpConnection::SystemStatusReference:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Reference");
                break;
            case LambdaModbusTcpConnection::SystemStatusRestartBlock:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Restart block");
                break;
            case LambdaModbusTcpConnection::SystemStatusReady:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Ready");
                break;
            case LambdaModbusTcpConnection::SystemStatusStartPumps:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Start pumps");
                break;
            case LambdaModbusTcpConnection::SystemStatusStartCompressor:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Start compressor");
                break;
            case LambdaModbusTcpConnection::SystemStatusPreRegulation:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Pre regulation");
                break;
            case LambdaModbusTcpConnection::SystemStatusRegulation:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Regulation");
                break;
            case LambdaModbusTcpConnection::SystemStatusNotUsed:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Not used");
                break;
            case LambdaModbusTcpConnection::SystemStatusCooling:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Cooling");
                break;
            case LambdaModbusTcpConnection::SystemStatusDefrosting:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Defrosting");
                break;
            }

            // Set heating and cooling states according to the system state
            thing->setStateValue(lambdaTCPHeatingOnStateTypeId, systemStatus == LambdaModbusTcpConnection::SystemStatusRegulation);
            thing->setStateValue(lambdaTCPCoolingOnStateTypeId, systemStatus == LambdaModbusTcpConnection::SystemStatusCooling);
        });


        // Holding registers
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::outdoorTemperatureChanged, thing, [thing](float outdoorTemperature){
            qCDebug(dcLambda()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
            thing->setStateValue(lambdaTCPOutdoorTemperatureStateTypeId, outdoorTemperature);
        });

        // JoOb: new power registers
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::powerDemandChanged, thing, [thing](float powerDemand){
            qCDebug(dcLambda()) << thing << "power demand changed" << powerDemand << "W";
            thing->setStateValue(lambdaTCPPowerDemandStateTypeId, powerDemand);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::powerActualChanged, thing, [thing](float powerActual){
            qCDebug(dcLambda()) << thing << "actual power consumption changed" << powerActual << "W";
            thing->setStateValue(lambdaTCPPowerActualStateTypeId, powerActual);
        });

        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::powerSetpointChanged, thing, [thing](float powerSetpoint){
            qCDebug(dcLambda()) << thing << "realized power setpoint changed" << powerSetpoint << "W";
            thing->setStateValue(lambdaTCPPowerSetpointStateTypeId, powerSetpoint);
        });

        

        m_connections.insert(thing, lambdaTCPTcpConnection);
        lambdaTCPTcpConnection->connectDevice();

        // FIXME: make async and check if this is really an alpha connect
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginLambda::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == lambdaTCPThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcLambda()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach (LambdaModbusTcpConnection *connection, m_connections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginLambda::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == lambdaTCPThingClassId && m_connections.contains(thing)) {
        LambdaModbusTcpConnection *connection = m_connections.take(thing);
        delete connection;
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginLambda::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    LambdaModbusTcpConnection *connection = m_connections.value(thing);

    if (!connection->connected()) {
        qCWarning(dcLambda()) << "Could not execute action. The modbus connection is currently not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }
}


