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


#define GPIO_ADDRESS_RELAIS 496

IntegrationPluginLambda::IntegrationPluginLambda()
{

}

void IntegrationPluginLambda::init()
{
    // TODO: Load possible system configurations for gpio pairs depending on well knwon platforms
}

void IntegrationPluginLambda::discoverThings(ThingDiscoveryInfo *info)
{
    if (!Gpio::isAvailable()) {
        qCWarning(dcLambda()) << "There are no GPIOs available on this plattform";
    }
    
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
        if (!Gpio::isAvailable()) {
            qCWarning(dcLambda()) << "There are no GPIOs available on this plattform";
        } else {
            // if CLS is true, relais shall be activated to ensure "EVU Freigabe" after init
            bool clsState = thing->stateValue(lambdaTCPControllableLocalSystemStateTypeId).toBool();
                        
            LpcInterface *lpcInterface = new LpcInterface(GPIO_ADDRESS_RELAIS, this);
            if (!lpcInterface->setup(clsState)) {
                qCWarning(dcLambda()) << "Setup" << thing << "failed because the GPIO could not be set up correctly.";
                //: Error message if LPC GPIOs setup failed
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Failed to set up the GPIO hardware interface."));
                return;
            }

            // Intially set values according to relais states
            thing->setStateValue(lambdaTCPRelais1StateStateTypeId, lpcInterface->gpio1()->value() == Gpio::ValueHigh);            

            // Reflect the GPIO change
            connect(lpcInterface, &LpcInterface::limitPowerConsumptionChanged, this, [thing, lpcInterface](){
                qCDebug(dcLambda()) << "GPIO changed to" << (lpcInterface->gpio1()->value() == Gpio::ValueHigh);
                thing->setStateValue(lambdaTCPRelais1StateStateTypeId, lpcInterface->gpio1()->value() == Gpio::ValueHigh);
            });        

            m_lpcInterfaces.insert(thing, lpcInterface);
        }

        QHostAddress hostAddress = QHostAddress(thing->paramValue(lambdaTCPThingIpAddressParamTypeId).toString());
        if (hostAddress.isNull()) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("No IP address given"));
            return;
        }

        quint16 port = thing->paramValue(lambdaTCPThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(lambdaTCPThingSlaveIdParamTypeId).toUInt();

        LambdaModbusTcpConnection *lambdaTCPTcpConnection = new LambdaModbusTcpConnection(hostAddress, port, slaveId, this);
        connect(info, &ThingSetupInfo::aborted, lambdaTCPTcpConnection, &LambdaModbusTcpConnection::deleteLater);
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::reachableChanged, this, [thing, lambdaTCPTcpConnection](bool status){
            qCDebug(dcLambda()) << "Connected changed to" << status << "for" << thing;
            if (status) {
                lambdaTCPTcpConnection->initialize();
            }

            thing->setStateValue(lambdaTCPConnectedStateTypeId, status);
            thing->setStateValue(lambdaTCPCurrentPowerStateTypeId, 0);
        });

        // Ambient module
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::ambientErrorNumberChanged, thing, [thing](qint16 ambientErrorNumber){
            qCDebug(dcLambda()) << thing << "ambient error number changed" << ambientErrorNumber;
            thing->setStateValue(lambdaTCPAmbientErrorNumberStateTypeId, ambientErrorNumber);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::ambientStateChanged, thing, [thing](LambdaModbusTcpConnection::AmbientState ambientState){
            qCDebug(dcLambda()) << thing << "operating state for ambient module" << ambientState;
            switch (ambientState) {
            case LambdaModbusTcpConnection::AmbientStateOff:
                thing->setStateValue(lambdaTCPAmbientStateStateTypeId, "Off");
                break;
            case LambdaModbusTcpConnection::AmbientStateAutomatik:
                thing->setStateValue(lambdaTCPAmbientStateStateTypeId, "Automatik");
                break;
            case LambdaModbusTcpConnection::AmbientStateManual:
                thing->setStateValue(lambdaTCPAmbientStateStateTypeId, "Manual");
                break;
            case LambdaModbusTcpConnection::AmbientStateError:
                thing->setStateValue(lambdaTCPAmbientStateStateTypeId, "Error");
                break;
            }
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::actualAmbientTemperatureChanged, thing, [thing](float actualAmbientTemperature){
            qCDebug(dcLambda()) << thing << "actual ambient temperature changed" << actualAmbientTemperature << "°C";
            thing->setStateValue(lambdaTCPActualAmbientTemperatureStateTypeId, actualAmbientTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::averageAmbientTemperatureChanged, thing, [thing](float averageAmbientTemperature){
            qCDebug(dcLambda()) << thing << "Arithmetic average temperature of the last 60 minutes changed" << averageAmbientTemperature << "°C";
            thing->setStateValue(lambdaTCPAverageAmbientTemperatureStateTypeId, averageAmbientTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::outdoorTemperatureChanged, thing, [thing](float outdoorTemperature){
            qCDebug(dcLambda()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
            thing->setStateValue(lambdaTCPOutdoorTemperatureStateTypeId, outdoorTemperature);
        });


        // E-Manager module
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::emanagerErrorNumberChanged, thing, [thing](qint16 emanagerErrorNumber){
            qCDebug(dcLambda()) << thing << "E-manager error number changed" << emanagerErrorNumber;
            thing->setStateValue(lambdaTCPEmanagerErrorNumberStateTypeId, emanagerErrorNumber);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::emanagerStateChanged, thing, [thing](LambdaModbusTcpConnection::EmanagerState emanagerState){
            qCDebug(dcLambda()) << thing << "E-Manager operating state changed" << emanagerState;
            switch (emanagerState) {
            case LambdaModbusTcpConnection::EmanagerStateOff:
                thing->setStateValue(lambdaTCPEmanagerStateStateTypeId, "Off");
                break;
            case LambdaModbusTcpConnection::EmanagerStateAutomatik:
                thing->setStateValue(lambdaTCPEmanagerStateStateTypeId, "Automatik");
                break;
            case LambdaModbusTcpConnection::EmanagerStateManual:
                thing->setStateValue(lambdaTCPEmanagerStateStateTypeId, "Manual");
                break;
            case LambdaModbusTcpConnection::EmanagerStateError:
                thing->setStateValue(lambdaTCPEmanagerStateStateTypeId, "Error");
                break;
            case LambdaModbusTcpConnection::EmanagerStateOffline:
                thing->setStateValue(lambdaTCPEmanagerStateStateTypeId, "Offline");
                break;
            }
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::actualPvSurplusChanged, thing, [thing](float actualPvSurplus){
            qCDebug(dcLambda()) << thing << "power demand changed" << actualPvSurplus << "W";
            thing->setStateValue(lambdaTCPActualPvSurplusStateTypeId, actualPvSurplus);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::currentPowerChanged, thing, [thing](float currentPower){
            qCDebug(dcLambda()) << thing << "actual power consumption changed" << currentPower << "W";
            thing->setStateValue(lambdaTCPCurrentPowerStateTypeId, currentPower);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::powerSetpointChanged, thing, [thing](float powerSetpoint){
            qCDebug(dcLambda()) << thing << "realized power setpoint changed" << powerSetpoint << "W";
            thing->setStateValue(lambdaTCPPowerSetpointStateTypeId, powerSetpoint);
        });


        // Heat pump module
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatpumpErrorStateChanged, thing, [thing](LambdaModbusTcpConnection::HeatpumpErrorState heatpumpErrorState){
            qCDebug(dcLambda()) << thing << "Heat pump error state changed" << heatpumpErrorState;
            switch (heatpumpErrorState) {
            case LambdaModbusTcpConnection::HeatpumpErrorStateNone:
                thing->setStateValue(lambdaTCPHeatpumpErrorStateStateTypeId, "None");
                break;
            case LambdaModbusTcpConnection::HeatpumpErrorStateMessage:
                thing->setStateValue(lambdaTCPHeatpumpErrorStateStateTypeId, "Message");
                break;
            case LambdaModbusTcpConnection::HeatpumpErrorStateWarning:
                thing->setStateValue(lambdaTCPHeatpumpErrorStateStateTypeId, "Warning");
                break;
            case LambdaModbusTcpConnection::HeatpumpErrorStateAlarm:
                thing->setStateValue(lambdaTCPHeatpumpErrorStateStateTypeId, "Alarm");
                break;
            case LambdaModbusTcpConnection::HeatpumpErrorStateFault:
                thing->setStateValue(lambdaTCPHeatpumpErrorStateStateTypeId, "Fault");
                break;
            }
        });        
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatpumpErrorNumberChanged, thing, [thing](qint16 heatpumpErrorNumber){
            qCDebug(dcLambda()) << thing << "E-manager error number changed" << heatpumpErrorNumber;
            thing->setStateValue(lambdaTCPHeatpumpErrorNumberStateTypeId, heatpumpErrorNumber);
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
                qCDebug(dcLambda()) << thing << "system status is now: 3 Ready Mode"; //JoOb
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Ready");
                break;
            case LambdaModbusTcpConnection::SystemStatusStartPumps:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Start pumps");
                break;
            case LambdaModbusTcpConnection::SystemStatusStartCompressor:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Start compressor");
                break;
            case LambdaModbusTcpConnection::SystemStatusPreRegulation:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Pre-regulation");
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
            case LambdaModbusTcpConnection::SystemStatusStopping:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Stopping");
                break;
            case LambdaModbusTcpConnection::SystemStatusFaultLock:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Fault lock");
                break;
            case LambdaModbusTcpConnection::SystemStatusAlarmBlock:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Alarm block");
                break;
            case LambdaModbusTcpConnection::SystemStatusErrorReset:
                thing->setStateValue(lambdaTCPSystemStatusStateTypeId, "Error reset");
                break;
            }

            // Set heating and cooling states according to the system state
            thing->setStateValue(lambdaTCPHeatingOnStateTypeId, systemStatus == LambdaModbusTcpConnection::SystemStatusRegulation);
            thing->setStateValue(lambdaTCPCoolingOnStateTypeId, systemStatus == LambdaModbusTcpConnection::SystemStatusCooling);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatpumpStateChanged, thing, [thing](LambdaModbusTcpConnection::HeatpumpState heatpumpState){
            qCDebug(dcLambda()) << thing << "Heat pump operating state" << heatpumpState;
            switch (heatpumpState) {
            case LambdaModbusTcpConnection::HeatpumpStateStby:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Standby");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateCh:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Central Heating");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateDhw:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Domestic Hot Water");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateCc:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Central Cooling");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateCirculate:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Circulate");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateDefrost:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Defrost");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateOff:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Off");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateFrost:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Frost");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateStbyFrost:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Standby Frost");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateNotUsed:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Not Used");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateSummer:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Summer");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateHoliday:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Holiday");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateError:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Error");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateWarning:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Warning");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateInfoMessage:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Info Message");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateTimeBlock:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Time Block");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateReleaseBlock:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Release Block");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateMinTempBlock:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "MinTemp Block");
                break;
            case LambdaModbusTcpConnection::HeatpumpStateFirmwareDownload:
                thing->setStateValue(lambdaTCPHeatpumpStateStateTypeId, "Firmware Download");
                break;
            }
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatpumpFlowTemperatureChanged, thing, [thing](float heatpumpFlowTemperature){
            qCDebug(dcLambda()) << thing << "heat pump flow temperature changed" << heatpumpFlowTemperature << "°C";
            thing->setStateValue(lambdaTCPHeatpumpFlowTemperatureStateTypeId, heatpumpFlowTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatpumpReturnTemperatureChanged, thing, [thing](float heatpumpReturnTemperature){
            qCDebug(dcLambda()) << thing << "heat pump return temperature changed" << heatpumpReturnTemperature << "°C";
            thing->setStateValue(lambdaTCPHeatpumpReturnTemperatureStateTypeId, heatpumpReturnTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::volumeFlowSinkChanged, thing, [thing](float volumeFlowSink){
            qCDebug(dcLambda()) << thing << "Volume flow heat sink changed" << volumeFlowSink << "l/min";
            thing->setStateValue(lambdaTCPVolumeFlowSinkStateTypeId, volumeFlowSink);
        });        
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::energySourceInletTemperatureChanged, thing, [thing](float energySourceInletTemperature){
            qCDebug(dcLambda()) << thing << "energy source inlet temperature changed" << energySourceInletTemperature << "°C";
            thing->setStateValue(lambdaTCPEnergySourceInletTemperatureStateTypeId, energySourceInletTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::energySourceOutletTemperatureChanged, thing, [thing](float energySourceOutletTemperature){
            qCDebug(dcLambda()) << thing << "energy source outlet temperature changed" << energySourceOutletTemperature << "°C";
            thing->setStateValue(lambdaTCPEnergySourceOutletTemperatureStateTypeId, energySourceOutletTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::volumeFlowSourceChanged, thing, [thing](float volumeFlowSource){
            qCDebug(dcLambda()) << thing << "Volume flow energy source changed" << volumeFlowSource << "l/min";
            thing->setStateValue(lambdaTCPVolumeFlowSourceStateTypeId, volumeFlowSource);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::compressorRatingChanged, thing, [thing](float compressorRating){
            qCDebug(dcLambda()) << thing << "Compressor unit rating changed" << compressorRating << "%";
            thing->setStateValue(lambdaTCPCompressorRatingStateTypeId, compressorRating);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::actualHeatingCapacityChanged, thing, [thing](float actualHeatingCapacity){
            qCDebug(dcLambda()) << thing << "Actual heating capacity changed" << actualHeatingCapacity << "kW";
            thing->setStateValue(lambdaTCPActualHeatingCapacityStateTypeId, actualHeatingCapacity);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::powerActualInverterChanged, thing, [thing](qint16 powerActualInverter){
            qCDebug(dcLambda()) << thing << "Frequency inverter actual power consumption changed" << powerActualInverter << "W";
            thing->setStateValue(lambdaTCPPowerActualInverterStateTypeId, powerActualInverter);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::coefficientOfPerformanceChanged, thing, [thing](float coefficientOfPerformance){
            qCDebug(dcLambda()) << thing << "Coefficient of performance changed" << coefficientOfPerformance << "%";
            thing->setStateValue(lambdaTCPCoefficientOfPerformanceStateTypeId, coefficientOfPerformance);
        });
        // 1-0-14 until 1-0-19 not connected
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::totalEnergyConsumedChanged, thing, [thing](float totalEnergyConsumed){
            qCDebug(dcLambda()) << thing << "Accumulated electrical energy consumption of compressor unit since last statistic reset" << totalEnergyConsumed << "Wh";
            thing->setStateValue(lambdaTCPTotalEnergyConsumedStateTypeId, totalEnergyConsumed);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::compressorTotalHeatOutputChanged, thing, [thing](float compressorTotalHeatOutput){
            qCDebug(dcLambda()) << thing << "Accumulated thermal energy output of compressor unit since last statistic reset" << compressorTotalHeatOutput << "Wh";
            thing->setStateValue(lambdaTCPCompressorTotalHeatOutputStateTypeId, compressorTotalHeatOutput);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::updateFinished, thing, [thing, lambdaTCPTcpConnection](){
            qCDebug(dcLambda()) << "Lambda heat pump - Update finished.";
            float energyInput = lambdaTCPTcpConnection->totalEnergyConsumed();
            float energyOutput = lambdaTCPTcpConnection->compressorTotalHeatOutput();
            float averageCOP = energyOutput / energyInput;
            qCDebug(dcLambda()) << thing << "Average coefficient of performance" << averageCOP;
            if (energyInput > 0) {
                thing->setStateValue(lambdaTCPAverageCoefficientOfPerformanceStateTypeId, averageCOP);
            }
        });        
        // 1-0-50 not connected

        // Boiler module
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::hotWaterTemperatureChanged, thing, [thing](float hotWaterTemperature){
            qCDebug(dcLambda()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
            thing->setStateValue(lambdaTCPHotWaterTemperatureStateTypeId, hotWaterTemperature);
        });

        // Buffer module        
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::bufferErrorNumberChanged, thing, [thing](qint16 bufferErrorNumber){
            qCDebug(dcLambda()) << thing << "Buffer error number changed" << bufferErrorNumber;
            thing->setStateValue(lambdaTCPBufferErrorNumberStateTypeId, bufferErrorNumber);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::bufferStateChanged, thing, [thing](LambdaModbusTcpConnection::BufferState bufferState){
            qCDebug(dcLambda()) << thing << "Buffer operating state changed" << bufferState;
            switch (bufferState) {
            case LambdaModbusTcpConnection::BufferStateStby:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Stby");
                break;
            case LambdaModbusTcpConnection::BufferStateHeating:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Heating");
                break;
            case LambdaModbusTcpConnection::BufferStateCooling:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Cooling");
                break;
            case LambdaModbusTcpConnection::BufferStateSummer:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Summer");
                break;
            case LambdaModbusTcpConnection::BufferStateFrost:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Frost");
                break;
            case LambdaModbusTcpConnection::BufferStateHoliday:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Holiday");
                break;
            case LambdaModbusTcpConnection::BufferStatePrioStop:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "PrioStop");
                break;
            case LambdaModbusTcpConnection::BufferStateError:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Error");
                break;
            case LambdaModbusTcpConnection::BufferStateOff:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "Off");
                break;
            case LambdaModbusTcpConnection::BufferStateStbyFrost:
                thing->setStateValue(lambdaTCPBufferStateStateTypeId, "StbyFrost");
                break;
            }
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::bufferTemperatureHighChanged, thing, [thing](float bufferTemperatureHigh){
            qCDebug(dcLambda()) << thing << "Actual temperature buffer high sensor changed" << bufferTemperatureHigh << "°C";
            thing->setStateValue(lambdaTCPBufferTemperatureHighStateTypeId, bufferTemperatureHigh);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::bufferTemperatureLowChanged, thing, [thing](float bufferTemperatureLow){
            qCDebug(dcLambda()) << thing << "Actual temperature buffer low sensor" << bufferTemperatureLow << "°C";
            thing->setStateValue(lambdaTCPBufferTemperatureLowStateTypeId, bufferTemperatureLow);
        });
        // 3-0-50 not connected
        
        // Heating circuit
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatingcircuitErrorNumberChanged, thing, [thing](qint16 heatingcircuitErrorNumber){
            qCDebug(dcLambda()) << thing << "Heating circuit error number changed" << heatingcircuitErrorNumber;
            thing->setStateValue(lambdaTCPHeatingcircuitErrorNumberStateTypeId, heatingcircuitErrorNumber);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatingcircuitStateChanged, thing, [thing](LambdaModbusTcpConnection::HeatingcircuitState heatingcircuitState){
            qCDebug(dcLambda()) << thing << "Heating circuit operating state changed" << heatingcircuitState;
            switch (heatingcircuitState) {
            case LambdaModbusTcpConnection::HeatingcircuitStateHeating:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Heating");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateEco:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Eco");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateCooling:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Cooling");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateFloordry:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Floordry");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateFrost:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Frost");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateMaxTemp:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "MaxTemp");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateError:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Error");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateService:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Service");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateHoliday:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Holiday");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateChSummer:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "ChSummer");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateCcWinter:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "CcWinter");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStatePrioStop:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "PrioStop");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateOff:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Off");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateReleaseOff:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "ReleaseOff");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateTimeOff:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "TimeOff");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateStby:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "Stby");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateStbyHeating:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "StbyHeating");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateStbyEco:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "StbyEco");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateStbyCooling:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "StbyCooling");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateStbyFrost:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "StbyFrost");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitStateStbyFloordry:
                thing->setStateValue(lambdaTCPHeatingcircuitStateStateTypeId, "StbyFloordry");
                break;
            }
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::flowTemperatureChanged, thing, [thing](float flowTemperature){
            qCDebug(dcLambda()) << thing << "heating circuit flow temperature changed" << flowTemperature << "°C";
            thing->setStateValue(lambdaTCPFlowTemperatureStateTypeId, flowTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::roomTemperatureChanged, thing, [thing](float roomTemperature){
            qCDebug(dcLambda()) << thing << "Actual temperature room device sensor changed" << roomTemperature << "°C";
            thing->setStateValue(lambdaTCPRoomTemperatureStateTypeId, roomTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::setpointFlowTemperatureChanged, thing, [thing](float setpointFlowTemperature){
            qCDebug(dcLambda()) << thing << "Setpoint temperature flow line changed" << setpointFlowTemperature << "°C";
            thing->setStateValue(lambdaTCPSetpointFlowTemperatureStateTypeId, setpointFlowTemperature);
        });
        connect(lambdaTCPTcpConnection, &LambdaModbusTcpConnection::heatingcircuitModeChanged, thing, [thing](LambdaModbusTcpConnection::HeatingcircuitMode heatingcircuitMode){
            qCDebug(dcLambda()) << thing << "Heating circuit operating mode changed" << heatingcircuitMode;
            switch (heatingcircuitMode) {
            case LambdaModbusTcpConnection::HeatingcircuitModeOff:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "Off");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeManual:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "Manual");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeAutomatik:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "Automatik");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeAutoHeating:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "AutoHeating");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeAutoCooling:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "AutoCooling");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeFrost:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "Frost");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeSummer:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "Summer");
                break;
            case LambdaModbusTcpConnection::HeatingcircuitModeFloorDry:
                thing->setStateValue(lambdaTCPHeatingcircuitModeStateTypeId, "FloorDry");
            }
        });
        // 5-0-50 until 5-0-52 not connected        

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
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
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
    if (thing->thingClassId() == lambdaTCPThingClassId) {
        if (m_connections.contains(thing)) {
            LambdaModbusTcpConnection *connection = m_connections.take(thing);
            delete connection;
        }
        if (m_lpcInterfaces.contains(thing)) {
            LpcInterface *lpcInterface = m_lpcInterfaces.take(thing);
            delete lpcInterface;
        }
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

    if (info->action().actionTypeId() == lambdaTCPActualPvSurplusActionTypeId) {
        double actualPvSurplus = info->action().paramValue(lambdaTCPActualPvSurplusActionActualPvSurplusParamTypeId).toDouble();
        qCDebug(dcLambda()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        
        // Set global variable m_demandPower in case ActualPvSurplus state changed
        connection->setDemandPower(actualPvSurplus);
    }

    LpcInterface *lpcInterface = m_lpcInterfaces.value(thing);
        if (!lpcInterface || !lpcInterface->isValid()) {
            qCWarning(dcLambda()) << "Failed to execute action. There is no interface available for" << thing;
        } else { 
            bool lpcRequest = false;
            bool clsState = false;
            bool gpioSetting = false;

            if (info->action().actionTypeId() == lambdaTCPActivateLpcActionTypeId) {
                lpcRequest = info->action().paramValue(lambdaTCPActivateLpcActionActivateLpcParamTypeId).toBool();
                clsState = thing->stateValue(lambdaTCPControllableLocalSystemStateTypeId).toBool();
                thing->setStateValue(lambdaTCPActivateLpcStateTypeId, lpcRequest);
                
                qCDebug(dcLambda()) << "LPC activation action, value:" << lpcRequest;
                
            } else if(info->action().actionTypeId() == lambdaTCPControllableLocalSystemActionTypeId) {                
                clsState = info->action().paramValue(lambdaTCPControllableLocalSystemActionControllableLocalSystemParamTypeId).toBool();
                thing->setStateValue(lambdaTCPControllableLocalSystemStateTypeId, clsState);
                lpcRequest = thing->stateValue(lambdaTCPActivateLpcStateTypeId).toBool();

                qCDebug(dcLambda()) << "CLS state action, value:" << clsState;
            }

            gpioSetting = clsState && !lpcRequest;
            if (!lpcInterface->setLimitPowerConsumption(gpioSetting)) {
                    qCWarning(dcLambda()) << "Failed to set LPC relais on" << thing << "to" << gpioSetting;
            }
        }
}


