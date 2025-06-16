/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2025, Consolinno Energy GmbH
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

#include <QtMath>

#include "integrationpluginfoxess.h"
#include "plugininfo.h"

#include "rseriesdiscovery.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include "platform/platformzeroconfcontroller.h"
#include "network/zeroconf/zeroconfservicebrowser.h"

IntegrationPluginFoxESS::IntegrationPluginFoxESS()
{

}

void IntegrationPluginFoxESS::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcFoxess()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(foxRSeriesThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcFoxess()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue("connected", false);
                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginFoxESS::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == foxRSeriesThingClassId) {
        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), 1, info);

        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master found. Please set up a modbus RTU master first."));
                return;
            }

            qCInfo(dcFoxess()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {

                QString name = supportedThings().findById(info->thingClassId()).displayName();
                ThingDescriptor descriptor(info->thingClassId(), name, "SN" + result.serialNumber);

                ParamList params{
                    {foxRSeriesThingSlaveAddressParamTypeId, result.modbusId},
                    {foxRSeriesThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {foxRSeriesThingSerialNumberParamTypeId, result.serialNumber},
                };
                descriptor.setParams(params);

                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Things existingThings = myThings().filterByThingClassId(foxRSeriesThingClassId).filterByParam(foxRSeriesThingSerialNumberParamTypeId, result.serialNumber);
                if (!existingThings.isEmpty()) {
                    descriptor.setThingId(existingThings.first()->id());
                }

                // Some remarks to the above code: This plugin gets copy-pasted to get the inverter and consumer SDM630. Since they are different plugins, myThings() won't contain the things
                // from these plugins. So currently you can add the same device once in each plugin.

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        discovery->startDiscovery();
        return;
    } else if (info->thingClassId() == foxEssThingClassId) {
        if (!hardwareManager()->zeroConfController()->available())
        {
            qCWarning(dcFoxess()) << "Error discovering FoxESS wallbox. Available:"  << hardwareManager()->zeroConfController()->available();
            info->finish(Thing::ThingErrorHardwareNotAvailable, "Thing discovery not possible");
            return;
        }
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
        qCDebug(dcFoxess()) << "Discovery: Network discovery finished. Found"
                              << discoveryReply->networkDeviceInfos().count() << "network devices";
            m_serviceBrowser = hardwareManager()->zeroConfController()->createServiceBrowser();
            foreach (const ZeroConfServiceEntry &service, m_serviceBrowser->serviceEntries()) {
                // qCDebug(dcFoxess()) << "mDNS service entry:" << service;
                if (service.hostName().contains("EVC-"))
                {
                    ThingDescriptor descriptor(foxEssThingClassId, "FoxESS Wallbox", service.hostAddress().toString());
                    NetworkDeviceInfo foxESSWallbox = discoveryReply->networkDeviceInfos().get(service.hostAddress());
                    qCDebug(dcFoxess()) << "MacAddress of WB:" << foxESSWallbox.macAddress();

                    if (service.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol) {
                        continue;
                    }

                    if (foxESSWallbox.macAddress().isNull()) {
                        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The wallbox was found, but the MAC address is invalid. Try searching again."));
                    }
                    Things existingThings = myThings().filterByThingClassId(foxEssThingClassId).filterByParam(foxEssThingMacAddressParamTypeId, foxESSWallbox.macAddress());
                    if (!existingThings.isEmpty())
                        descriptor.setThingId(existingThings.first()->id());

                    ParamList params;
                    params << Param(foxEssThingIpAddressParamTypeId, service.hostAddress().toString());
                    params << Param(foxEssThingMdnsNameParamTypeId, service.name());
                    params << Param(foxEssThingPortParamTypeId, service.port());
                    params << Param(foxEssThingMacAddressParamTypeId, foxESSWallbox.macAddress());
                    params << Param(foxEssThingModbusIdParamTypeId, 1);
                    descriptor.setParams(params);
                    info->addThingDescriptor(descriptor);
                }
            }
            discoveryReply->deleteLater();
            info->finish(Thing::ThingErrorNoError);
        });
    }
}

void IntegrationPluginFoxESS::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxess()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == foxRSeriesThingClassId) {

        uint address = thing->paramValue(foxRSeriesThingSlaveAddressParamTypeId).toUInt();
        if (address > 247) {
            qCWarning(dcFoxess()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(foxRSeriesThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcFoxess()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcFoxess()) << "Already have a FoxESS SunSpec connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        RSeriesModbusRtuConnection *connection = new RSeriesModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &RSeriesModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue("connected", reachable);
            if (reachable) {
                qCDebug(dcFoxess()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
                connection->initialize();
            } else {
                qCDebug(dcFoxess()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
                thing->setStateValue("currentPower", 0);
                thing->setStateValue("operatingState", "Off");
            }
        });

        connect(connection, &RSeriesModbusRtuConnection::initializationFinished, info, [info, thing, connection](bool success) {
            qCWarning(dcFoxess()) << "Setup: Initialization finished";
            if (success) {
                QString deviceModel = connection->deviceModel();
                thing->setName("FoxESS " + deviceModel);
                if (deviceModel == "R75") {
                    thing->setStateMaxValue("productionLimit", 75000);
                } else if (deviceModel == "R100") {
                    thing->setStateMaxValue("productionLimit", 100000);
                } else if (deviceModel == "R110") {
                    thing->setStateMaxValue("productionLimit", 110000);
                }
                info->finish(Thing::ThingErrorNoError);
            } else {
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The inverter is not responding."));
            }
        });

        connect(connection, &RSeriesModbusRtuConnection::initializationFinished, thing, [thing, connection](bool success) {
            qCWarning(dcFoxess()) << "Setup: Initialization finished";
            if (success) {
                thing->setStateValue("firmwareVersion", connection->firmware());
                thing->setStateValue("serialNumber", connection->serialnumber());
            }
        });

        // Handle property changed signals
        connect(connection, &RSeriesModbusRtuConnection::updateFinished, thing, [=](){
            qCDebug(dcFoxess()) << "Update finished for" << thing->name();

            // Calculate inverter energy
            qint16 producedEnergySf = connection->inverterProducedEnergySf();
            quint32 producedEnergy = connection->inverterProducedEnergy();
            double calculatedEnergy = static_cast<double> ((producedEnergy * qPow(10, producedEnergySf)) / 1000);
            thing->setStateValue("totalEnergyProduced", calculatedEnergy);
            
            // Set operating state
            setOperatingState(thing, connection->inverterState());

            // Calculate inverter power
            qint16 currentPowerSf = connection->inverterCurrentPowerSf();
            qint16 currentPower = connection->inverterCurrentPower();
            double calculatedPower = static_cast<double> (-1 * qFabs(currentPower) * qPow(10, currentPowerSf));
            thing->setStateValue("currentPower", calculatedPower);

            // Calculate inverter frequency
            qint16 frequencySf = connection->inverterFrequencySf();
            quint32 frequency = connection->inverterFrequency();
            double calculatedFrequency = static_cast<double> (frequency * qPow(10, frequencySf));
            thing->setStateValue("frequency", calculatedFrequency);

            // Calculate max production limit
            qint16 productionLimitSf = connection->productionLimitSf();
            quint16 productionLimit = connection->productionLimit();
            quint16 calculatedProductionPercent = productionLimit * qPow(10, productionLimitSf);

            QString deviceModel = connection->deviceModel();
            if (deviceModel == "R75") {
                thing->setStateValue("productionLimit", calculatedProductionPercent * 750);
            } else if (deviceModel == "R100") {
                thing->setStateValue("productionLimit", calculatedProductionPercent * 1000);
            } else if (deviceModel == "R110") {
                thing->setStateValue("productionLimit", calculatedProductionPercent * 1100);
            }

        });

        m_rtuConnections.insert(thing, connection);
    } else if (thing->thingClassId() == foxEssThingClassId) {
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress =
                MacAddress(thing->paramValue(foxEssThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcFoxess())
                    << "Failed to set up FoxESS wallbox because the MAC address is not valid:"
                    << thing->paramValue(foxEssThingMacAddressParamTypeId).toString()
                    << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter,
                         QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device "
                                    "to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is
        // reachable without polling on our own. In this call, nymea is checking a list for known
        // mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor =
                hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip
        // address and is set to not reachable
        m_monitors.insert(thing, monitor);
    
        qCDebug(dcFoxess()) << "Monitor reachable" << monitor->reachable()
                              << thing->paramValue(foxEssThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(hardwareManager()->networkDeviceDiscovery(),
                &NetworkDeviceDiscovery::cacheUpdated, info, [this, info]() {
                    if (!m_setupTcpConnectionRunning) {
                        m_setupTcpConnectionRunning = true;
                        setupTcpConnection(info);
                    }
            });
        }
    }
}

void IntegrationPluginFoxESS::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcFoxess()) << "Setup TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    uint port = thing->paramValue(foxEssThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(foxEssThingModbusIdParamTypeId).toUInt();
    FoxESSModbusTcpConnection *connection = new FoxESSModbusTcpConnection(
            monitor->networkDeviceInfo().address(), port, modbusId, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Is this needed? How can setup be aborted at this point?

        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcFoxess()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcFoxess()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(foxEssConnectedStateTypeId).toBool()) {
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    // Initialize if device is reachable again, otherwise set connected state to false
    // and power to 0
    connect(connection, &FoxESSModbusTcpConnection::reachableChanged, thing,
            [this, connection, thing](bool reachable) {
                qCDebug(dcFoxess()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(foxEssConnectedStateTypeId, false);
                    thing->setStateValue(foxEssCurrentPowerStateTypeId, 0);
                }
    });

    // Initialization has finished
    connect(connection, &FoxESSModbusTcpConnection::initializationFinished, thing,
       [this, connection, thing](bool success) {
           thing->setStateValue(foxEssConnectedStateTypeId, success);

           if (success) {
                // Set basic info about device
               qCDebug(dcFoxess()) << "FoxESS wallbox initialized.";
               quint16 firmware = connection->firmwareVersion();
               QString majorVersion = QString::number((firmware & 0xFF00) >> 8);
               QString minorVersion = QString::number(firmware & 0xFF);
               thing->setStateValue(foxEssFirmwareVersionStateTypeId,
                                    majorVersion+"."+minorVersion);
               thing->setStateValue(foxEssSerialNumberStateTypeId,
                                    connection->serialNumber());

               qCDebug(dcFoxess()) << "Max Supported Power is" << connection->maxSupportedPower() << "kW";
               if (connection->maxSupportedPower() == 11) {
                   qCDebug(dcFoxess()) << "Changed max current to 16"; 
                   thing->setStateMaxValue(foxEssMaxChargingCurrentStateTypeId, 16);
               } else {
                   qCDebug(dcFoxess()) << "Changed max current to 32"; 
                   thing->setStateMaxValue(foxEssMaxChargingCurrentStateTypeId, 32);
               }
           } else {
               qCDebug(dcFoxess()) << "FoxESS wallbox initialization failed.";
               // Try to reconnect to device
               connection->reconnectDevice();
           }
    });

    // Car has been plugged in or unplugged
    connect(connection, &FoxESSModbusTcpConnection::plugStatusChanged, thing, [thing](quint16 state) {
        if (state == 0)
        {
            // not connected
            qCDebug(dcFoxess()) << "Plug not connected";
            thing->setStateValue(foxEssPluggedInStateTypeId, false);
        } else {
            // connected
            qCDebug(dcFoxess()) << "Plug connected";
            thing->setStateValue(foxEssPluggedInStateTypeId, true);
        }
    });

    // Set state for phase current A
    connect(connection, &FoxESSModbusTcpConnection::currentPhaseAChanged, thing, [thing](float current) {
        qCDebug(dcFoxess()) << "Current Phase A changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseAStateTypeId, current);
    });

    // Set state for phase current B
    connect(connection, &FoxESSModbusTcpConnection::currentPhaseBChanged, thing, [thing](float current) {
        qCDebug(dcFoxess()) << "Current Phase B changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseBStateTypeId, current);
    });

    // Set state for phase current C
    connect(connection, &FoxESSModbusTcpConnection::currentPhaseCChanged, thing, [thing](float current) {
        qCDebug(dcFoxess()) << "Current Phase C changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseCStateTypeId, current);
    });

    // Set state for current power
    connect(connection, &FoxESSModbusTcpConnection::currentPowerChanged, thing, [thing](float power) {
        qCDebug(dcFoxess()) << "Current Power changed to" << power << "kW";
        thing->setStateValue(foxEssCurrentPowerStateTypeId, power*1000);
    });

    // IMPORTANT: as of August 2024, the registers for sessionEnergy and totalConsumedEnergy
    // are mixed up in the data sheet
    // Set state for session energy
    connect(connection, &FoxESSModbusTcpConnection::sessionEnergyConsumedChanged, thing, [thing](float energy) {
        qCDebug(dcFoxess()) << "Session energy changed to" << energy << "kWh";
        thing->setStateValue(foxEssSessionEnergyStateTypeId, energy);
    });

    // Set state for total consumed energy
    connect(connection, &FoxESSModbusTcpConnection::totalEnergyChanged, thing, [thing](float energy) {
        qCDebug(dcFoxess()) << "Total energy changed to" << energy << "kWh";
        thing->setStateValue(foxEssTotalEnergyConsumedStateTypeId, energy);
    });

    // Check if alarm info has changed
    connect(connection, &FoxESSModbusTcpConnection::alarmInfoChanged, thing, [thing](quint16 code) {
        qCDebug(dcFoxess()) << "Alarm info changed to" << code;
        QMap<int, QString> alarmInfoMap = {
            {0, "Card reader"}, {1, "Phase cutting box"},
            {2, "Phase loss"}
        };

        QString warningMessage = "";
        for (int i = 0; i <= 2; i++)
        {
            if (((code >> i) & 0x1) == 0x1)
            {
                warningMessage.append(alarmInfoMap[i]);
            }
        }
        qCDebug(dcFoxess()) << "Alaram code:" << code << "Alarm info:" << warningMessage;
        if (warningMessage != "")
        {
            thing->setStateValue(foxEssAlarmInfoStateTypeId, warningMessage);
        } else {
            thing->setStateValue(foxEssAlarmInfoStateTypeId, "No Error");
        }
    });

    // Check if fault info has changed
    connect(connection, &FoxESSModbusTcpConnection::faultInfoChanged, thing, [thing](quint32 code) {
        qCDebug(dcFoxess()) << "Fault info changed to" << code;
        QMap<int, QString> faultInfoMap = {
            {0, "Emergency stop"}, {1, "Overvoltage"},
            {2, "Undervoltage"}, {3, "Overcurrent"},
            {4, "Charge port temperature"}, {5, "PE grounding"},
            {6, "Leakage current"}, {7, "Frequency"},
            {8, "CP"}, {9, "Connector"},
            {10, "AC contactor"}, {11, "Electronic lock"},
            {12, "Breaker"}, {13, "CC"},
            {14, "Ext. meter communiction"}, {15, "Metering chip"},
            {16, "Environment temperature"}, {17, "Access control"},
        };
        QString warningMessage = "";
        for (int i = 0; i <= 17; i++)
        {
            if (((code >> i) & 0x1) == 0x1)
            {
                warningMessage.append(faultInfoMap[i]);
            }
        }
        qCDebug(dcFoxess()) << "Fault code:" << code << "Fault info:" << warningMessage;
        if (warningMessage != "")
        {
            thing->setStateValue(foxEssFaultInfoStateTypeId, warningMessage);
        } else {
            thing->setStateValue(foxEssFaultInfoStateTypeId, "No Error");
        }
    });

    // Check if work mode has changed
    connect(connection, &FoxESSModbusTcpConnection::workModeChanged, [this, thing, connection](quint32 mode) {
        qCDebug(dcFoxess()) << "Work mode changed to" << mode << ". Make sure it is in controlled mode.";
        // 0x3000 can not be written with FC 0x06
        // Workaround to write work mode register with overwriting maxChargeCurrent
        quint32 maxCurrent = mode & 0x0000FFFF;
        mode = (mode & 0xFFFF0000) >> 16;
        qCDebug(dcFoxess()) << "Masked work mode:" << mode << "Masked current: " << maxCurrent;
        if (mode != 0)
        {
            qCDebug(dcFoxess()) << "Setting workmode + maxChargeCurrent to" << (maxCurrent & 0x0000FFFF);
            QModbusReply *reply = connection->setWorkMode(maxCurrent & 0x0000FFFF);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                   qCDebug(dcFoxess()) << "Successfully send command to set mode to 'controlled mode'";
                }
            });
        }
    });

    // Check if deviceStatus has changed
    connect(connection, &FoxESSModbusTcpConnection::deviceStatusChanged, [this, thing](quint16 state) {
        qCDebug(dcFoxess()) << "Device status changed to" << state;

        switch (state) {
        case FoxESSModbusTcpConnection::EVCStatusIdle:
            thing->setStateValue(foxEssStateStateTypeId, "Available");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusConnected:
            thing->setStateValue(foxEssStateStateTypeId, "Connected");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusStarting:
            thing->setStateValue(foxEssStateStateTypeId, "Starting");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusCharging:
            thing->setStateValue(foxEssStateStateTypeId, "Charging");
            thing->setStateValue(foxEssChargingStateTypeId, true);
            break;
        case FoxESSModbusTcpConnection::EVCStatusPausing:
            thing->setStateValue(foxEssStateStateTypeId, "Paused");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusFinishing:
            thing->setStateValue(foxEssStateStateTypeId, "Finished");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusFaulted:
            thing->setStateValue(foxEssStateStateTypeId, "Faulted");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        }
    });

    connect(m_chargeLimitTimer, &QTimer::timeout, this, [this, thing, connection]() {
        qCDebug(dcFoxess()) << "m_chargeLimitTimer timeout.";
        float currentInApp = thing->stateValue(foxEssMaxChargingCurrentStateTypeId).toFloat();
        int phaseCount = thing->stateValue(foxEssPhaseCountStateTypeId).toUInt();
        setMaxCurrent(connection, currentInApp, phaseCount);
    });

    // Check if update has finished
    connect(connection, &FoxESSModbusTcpConnection::updateFinished, thing, [this, thing, connection]() {
        qCDebug(dcFoxess()) << "Update finished";
        float currentPhaseA = thing->stateValue(foxEssCurrentPhaseAStateTypeId).toFloat();
        float currentPhaseB = thing->stateValue(foxEssCurrentPhaseBStateTypeId).toFloat();
        float currentPhaseC = thing->stateValue(foxEssCurrentPhaseCStateTypeId).toFloat();

        // Set state count
        int phaseCount = 0;
        if (currentPhaseA > 0)
            phaseCount++;
        if (currentPhaseB > 0)
            phaseCount++;
        if (currentPhaseC > 0)
            phaseCount++;
        thing->setStateValue(foxEssPhaseCountStateTypeId, qMax(phaseCount,1));

        // TODO: Test working
        // If state is Charging, but power is false -> stop charging - DONE
        // if state available, connected, starting and power is true -> start charging - DONE
        QString state = thing->stateValue(foxEssStateStateTypeId).toString();
        bool power = thing->stateValue(foxEssPowerStateTypeId).toBool();
        if ((state == "Charging") && (power == false)) 
        {
            toggleCharging(connection, false);
        }

        if ((power == true) && ((state == "Available") ||
                                (state == "Connected") ||
                                (state == "Starting")  ||
                                (state == "Paused")    ||
                                (state == "Finished")))
        {
            toggleCharging(connection, true);
        }
        
        if (state == "Charging") {
            if (!m_chargeLimitTimer->isActive()) {
                qCDebug(dcFoxess()) << "State is charging; Starting m_chargeLimitTimer.";
                m_chargeLimitTimer->start();
            }
        } else {
            if (m_chargeLimitTimer->isActive()) {
                qCDebug(dcFoxess()) << "State is not charging; Stopping m_chargeLimitTimer.";
                m_chargeLimitTimer->stop();
            }
        }

        // Make current of wallbox follow app
        if ((state == "Charging") && (power == true)) {
            qCDebug(dcFoxess()) << "Charging, but current is not corret. Correcting";
            double meanCurrent = (currentPhaseA + currentPhaseB + currentPhaseC) / phaseCount;
            float currentInApp = thing->stateValue(foxEssMaxChargingCurrentStateTypeId).toFloat();
            if ((meanCurrent > currentInApp+2) || (meanCurrent < currentInApp-2)) {
                setMaxCurrent(connection, currentInApp, phaseCount);
            }
        }
    });
    
    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginFoxESS::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == foxRSeriesThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcFoxess()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(RSeriesModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    } else if (thing->thingClassId() == foxEssThingClassId) {
        if (!m_chargeLimitTimer)
            m_chargeLimitTimer = new QTimer(this);
        m_chargeLimitTimer->setInterval(45*1000);

        if (!m_pluginTimer) {
            qCDebug(dcFoxess()) << "Starting plugin timer..";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                qCDebug(dcFoxess()) << "Updating FoxESS EVC..";
                foreach (FoxESSModbusTcpConnection *connection, m_tcpConnections) {
                    connection->update();
                }
            });
        }
    }
}

void IntegrationPluginFoxESS::executeAction(ThingActionInfo *info) 
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == foxRSeriesThingClassId) {
        RSeriesModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (info->action().actionTypeId() == foxRSeriesProductionLimitActionTypeId) {
            uint productionLimit = info->action().paramValue(foxRSeriesProductionLimitActionProductionLimitParamTypeId).toUInt();
            qint16 scaleFactor = -1 * connection->productionLimitSf();
            quint16 calculatedProductionLimit = (productionLimit / 1000) * qPow(10, scaleFactor);
            ModbusRtuReply *reply = connection->setProductionLimit(calculatedProductionLimit);
            connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
            connect(reply, &ModbusRtuReply::finished, this, [reply, thing, info, productionLimit]() {
                if (reply->error() == ModbusRtuReply::NoError) {
                    qCDebug(dcFoxess()) << "Successfully set production limit";
                    thing->setStateValue("productionLimit", productionLimit);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcFoxess()) << "Error setting production limit";
                }
            });
        }
    } else if (thing->thingClassId() == foxEssThingClassId) {
            FoxESSModbusTcpConnection *connection = m_tcpConnections.value(thing);
            if (info->action().actionTypeId() == foxEssPowerActionTypeId)
            {
                qCDebug(dcFoxess()) << "Start / Stop charging";
                bool power = info->action().paramValue(foxEssPowerActionPowerParamTypeId).toBool();
                toggleCharging(connection, power);
                thing->setStateValue(foxEssPowerStateTypeId, power);
                info->finish(Thing::ThingErrorNoError);
            }

            if (info->action().actionTypeId() == foxEssMaxChargingCurrentActionTypeId) {
                float maxCurrent = info->action().paramValue(foxEssMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toFloat();
                int phaseCount = thing->stateValue(foxEssPhaseCountStateTypeId).toUInt();
                thing->setStateValue(foxEssMaxChargingCurrentStateTypeId, maxCurrent);
                setMaxCurrent(connection, maxCurrent, phaseCount);
                info->finish(Thing::ThingErrorNoError);
            }
        }
}

void IntegrationPluginFoxESS::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == foxRSeriesThingClassId) {
        if (thing->thingClassId() == foxRSeriesThingClassId && m_rtuConnections.contains(thing)) {
            qCDebug(dcFoxess()) << "Removing rtu connection";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (myThings().isEmpty() && m_pluginTimer) {
            qCDebug(dcFoxess()) << "Deleting plugin timer";
            hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
            m_pluginTimer = nullptr;
        }
    } else if (thing->thingClassId() == foxEssThingClassId) {
        if (m_monitors.contains(thing)) {
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }

        if (m_tcpConnections.contains(thing)) {
            FoxESSModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        }

        if (myThings().isEmpty() && m_chargeLimitTimer) {
            m_chargeLimitTimer->stop();
            delete m_chargeLimitTimer;
            m_chargeLimitTimer = nullptr;
        }

        if (myThings().isEmpty() && m_pluginTimer) {
            hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
            m_pluginTimer = nullptr;
        }
    }
}

void IntegrationPluginFoxESS::toggleCharging(FoxESSModbusTcpConnection *connection, bool power)
{
    quint16 startCharging = 1;
    quint16 stopCharging = 2;
    QModbusReply *reply = connection->setChargingControl(power ? startCharging : stopCharging);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, connection, power, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
           qCDebug(dcFoxess()) << "Successfully set charge control";
        } else {
            qCDebug(dcFoxess()) << "Toggle charge was not sent successfully";
            // toggleCharging(connection, power);
        }
    });
}

void IntegrationPluginFoxESS::setMaxCurrent(FoxESSModbusTcpConnection *connection, float maxCurrent, int phaseCount)
{
    float maxPower = connection->maxChargePower();
    if (maxCurrent < 7)
        maxCurrent = 7;
    qCDebug(dcFoxess()) << "Setting maxChargeCurrent to" << maxCurrent;
    qCDebug(dcFoxess()) << "Setting maxChargePower to" << maxPower;
    maxPower = (230 * phaseCount * maxCurrent) / 1000;
    qCDebug(dcFoxess()) << "Calculated power is" << maxPower;
    maxCurrent = maxPower;
    QModbusReply *reply = connection->setMaxChargeCurrent(maxCurrent, maxPower);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, connection, maxCurrent, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
           qCDebug(dcFoxess()) << "Successfully set maximum charging current";
        } else {
            qCDebug(dcFoxess()) << "Setting max current was not successfull";
            // setMaxCurrent(connection, maxCurrent);
        }
    });
}

void IntegrationPluginFoxESS::setOperatingState(Thing *thing, RSeriesModbusRtuConnection::OperatingState state)
{
    switch (state) {
        case RSeriesModbusRtuConnection::OperatingStateOff:
            thing->setStateValue("operatingState", "Off");
            break;
        case RSeriesModbusRtuConnection::OperatingStateSleeping:
            thing->setStateValue("operatingState", "Sleeping");
            break;
        case RSeriesModbusRtuConnection::OperatingStateStarting:
            thing->setStateValue("operatingState", "Starting");
            break;
        case RSeriesModbusRtuConnection::OperatingStateMppt:
            thing->setStateValue("operatingState", "MPPT");
            break;
        case RSeriesModbusRtuConnection::OperatingStateThrottled:
            thing->setStateValue("operatingState", "Throttled");
            break;
        case RSeriesModbusRtuConnection::OperatingStateShuttingDown:
            thing->setStateValue("operatingState", "ShuttingDown");
            break;
        case RSeriesModbusRtuConnection::OperatingStateFault:
            thing->setStateValue("operatingState", "Fault");
            break;
        case RSeriesModbusRtuConnection::OperatingStateStandby:
            thing->setStateValue("operatingState", "Standby");
            break;
    }
}
