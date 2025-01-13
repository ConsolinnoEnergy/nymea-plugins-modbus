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

#include "integrationpluginkacosunspec.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

#include <QtMath>

#include "kaconh3discovery.h"

IntegrationPluginKacoSunSpec::IntegrationPluginKacoSunSpec()
{

}

void IntegrationPluginKacoSunSpec::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcKacoSunSpec()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(kacosunspecInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcKacoSunSpec()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(kacosunspecInverterRTUConnectedStateTypeId, false);
                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginKacoSunSpec::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == kacosunspecInverterTCPThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcKacoSunSpec()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }
        ThingClass thingClass = supportedThings().findById(info->thingClassId()); // TODO can this be done easier?
        qCDebug(dcKacoSunSpec()) << "Starting network discovery...";
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=](){
            qCDebug(dcKacoSunSpec()) << "Discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "devices";
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
                qCDebug(dcKacoSunSpec()) << networkDeviceInfo;

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
                params << Param(kacosunspecInverterTCPThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcKacoSunSpec()) << "Found already existing" << thingClass.name() << "inverter:" << existingThing->name() << networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcKacoSunSpec()) << "Found new" << thingClass.name() << "inverter";
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });

    } else if (info->thingClassId() == kacosunspecInverterRTUThingClassId) {
        qCDebug(dcKacoSunSpec()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint modbusId = info->params().paramValue(kacosunspecInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (modbusId > 247 || modbusId < 3) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 3 and 247."));
            return;
        }

        QList<ModbusRtuMaster*> candidateMasters;
        foreach (ModbusRtuMaster *master, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
                candidateMasters.append(master);
        }

        foreach (ModbusRtuMaster *modbusMaster, candidateMasters) {
            qCDebug(dcKacoSunSpec()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            QUuid modbusMasterUuid{modbusMaster->modbusUuid()};

            ThingDescriptor descriptor(info->thingClassId(), "Kaco SunSpec Inverter", QString::number(modbusId) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(kacosunspecInverterRTUThingSlaveAddressParamTypeId, modbusId);
            params << Param(kacosunspecInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);

            // Reconfigure not working when changing modbus ID right now. 
            // The existing device should be detected by checking a hardware serial (analog to MAC address in the TCP case)
            // This is not possible right now, as the serial is only available when the device is connected
            // --> Only practical when a real Modbus RTU discovery is implemented
            // However, this correctly prevents the user from adding the same device twice, as the UI will 
            // not show descriptors for already configured devices in the discovery list (they are only shown when using reconfigure)
            Thing *existingThing = myThings().findByParams(descriptor.params());
               if (existingThing) {
                   qCDebug(dcKacoSunSpec()) << "Found already configured inverter:" << existingThing->name() << modbusMaster->serialPort();
                   descriptor.setThingId(existingThing->id());
               } else {
                   qCDebug(dcKacoSunSpec()) << "Found Kaco SunSpec inverter on modbus master" << modbusMaster->serialPort() << "with modbus ID" << modbusId;
               }

            info->addThingDescriptor(descriptor);
        }
        qCDebug(dcKacoSunSpec()) << "Found" << info->thingDescriptors().count() << " Kaco SunSpec inverters";
        
        info->finish(Thing::ThingErrorNoError);
    } else if (info->thingClassId() == kaconh3ThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcKacoSunSpec()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }
        ThingClass thingClass = supportedThings().findById(info->thingClassId());
        qCDebug(dcKacoSunSpec()) << "Starting network discovery...";
        KacoNH3Discovery *discovery = new KacoNH3Discovery(hardwareManager()->networkDeviceDiscovery(), 502, 3, info);
        connect(discovery, &KacoNH3Discovery::discoveryFinished, discovery, &KacoNH3Discovery::deleteLater);
        connect(discovery, &KacoNH3Discovery::discoveryFinished, info, [=](){
            qCDebug(dcKacoSunSpec()) << "Discovery finished. Found Kaco device.";
            foreach (const KacoNH3Discovery::KacoNH3DiscoveryResult &result, discovery->discoveryResults()) {
                QString description = result.networkDeviceInfo.macAddress() + " - " + result.networkDeviceInfo.address().toString();

                ThingDescriptor descriptor(info->thingClassId(), "Kaco NH3", description);
                ParamList params;
                params << Param(kaconh3ThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcKacoSunSpec()) << "Found already existing" << thingClass.name() << "inverter:" << existingThing->name() << result.networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcKacoSunSpec()) << "Found new" << thingClass.name() << "inverter";
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });
        discovery->startDiscovery();
    }
}

void IntegrationPluginKacoSunSpec::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcKacoSunSpec()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == kacosunspecInverterTCPThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcKacoSunSpec()) << "Already have a Kaco SunSpec connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_tcpConnections.take(thing);
        } else {
            qCDebug(dcKacoSunSpec()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_scalefactors.contains(thing))
            m_scalefactors.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(kacosunspecInverterTCPThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcKacoSunSpec()) << "Failed to set up Kaco SunSpec inverter because the MAC address is not valid:" << thing->paramValue(kacosunspecInverterTCPThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
        });

        uint port = thing->paramValue(kacosunspecInverterTCPThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(kacosunspecInverterTCPThingSlaveIdParamTypeId).toUInt();

        KacoSunSpecModbusTcpConnection *connection = new KacoSunSpecModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        connection->setTimeout(4500);
        connect(info, &ThingSetupInfo::aborted, connection, &KacoSunSpecModbusTcpConnection::deleteLater);

        connect(connection, &KacoSunSpecModbusTcpConnection::reachableChanged, this, [connection, thing](bool reachable){
            qCDebug(dcKacoSunSpec()) << "Reachable state changed" << reachable;
            if (reachable) {
                connection->initialize();
            } else {
                thing->setStateValue(kacosunspecInverterTCPConnectedStateTypeId, false);
                thing->setStateValue(kacosunspecInverterTCPCurrentPowerStateTypeId, 0);
                thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Off");
            }
        });

        connect(connection, &KacoSunSpecModbusTcpConnection::initializationFinished, info, [this, thing, connection, monitor, info](bool success){
            if (success) {
                qCDebug(dcKacoSunSpec()) << "Kaco SunSpec inverter initialized.";
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

        connect(connection, &KacoSunSpecModbusTcpConnection::initializationFinished, this, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue(kacosunspecInverterTCPConnectedStateTypeId, true);
                connection->update();
            }
        });

        // Handle property changed signals
        connect(connection, &KacoSunSpecModbusTcpConnection::activePowerSfChanged, this, [this, thing](qint16 activePowerSf){
            qCDebug(dcKacoSunSpec()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &KacoSunSpecModbusTcpConnection::activePowerChanged, this, [this, thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * qPow(10, m_scalefactors.value(thing).powerSf);
            qCDebug(dcKacoSunSpec()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacosunspecInverterTCPCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &KacoSunSpecModbusTcpConnection::totalEnergyProducedSfChanged, this, [this, thing](qint16 totalEnergyProducedSf){
            qCDebug(dcKacoSunSpec()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &KacoSunSpecModbusTcpConnection::totalEnergyProducedChanged, this, [this, thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = (totalEnergyProduced * qPow(10, m_scalefactors.value(thing).energySf)) / 1000.0;
            qCDebug(dcKacoSunSpec()) << "Inverter total energy produced changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(kacosunspecInverterTCPTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });

        connect(connection, &KacoSunSpecModbusTcpConnection::operatingStateChanged, this, [this, thing](KacoSunSpecModbusTcpConnection::OperatingState operatingState){
            qCDebug(dcKacoSunSpec()) << "Inverter operating state recieved" << operatingState;
            setOperatingState(thing, operatingState);
        });

        connection->connectDevice();

        return;
    } else if (thing->thingClassId() == kacosunspecInverterRTUThingClassId) {

        uint address = thing->paramValue(kacosunspecInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address < 3) {
            qCWarning(dcKacoSunSpec()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 3 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(kacosunspecInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcKacoSunSpec()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcKacoSunSpec()) << "Already have a Kaco SunSpec connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (m_scalefactors.contains(thing))
            m_scalefactors.remove(thing);

        KacoSunSpecModbusRtuConnection *connection = new KacoSunSpecModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &KacoSunSpecModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(kacosunspecInverterRTUConnectedStateTypeId, reachable);
            if (reachable) {
                qCDebug(dcKacoSunSpec()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCDebug(dcKacoSunSpec()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
                thing->setStateValue(kacosunspecInverterRTUCurrentPowerStateTypeId, 0);
                thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Off");
            }
        });

        // Handle property changed signals
        connect(connection, &KacoSunSpecModbusRtuConnection::activePowerSfChanged, this, [this, thing](qint16 activePowerSf){
            qCDebug(dcKacoSunSpec()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &KacoSunSpecModbusRtuConnection::activePowerChanged, this, [this, thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * qPow(10, m_scalefactors.value(thing).powerSf);
            qCDebug(dcKacoSunSpec()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacosunspecInverterRTUCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &KacoSunSpecModbusRtuConnection::totalEnergyProducedSfChanged, this, [this, thing](qint16 totalEnergyProducedSf){
            qCDebug(dcKacoSunSpec()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &KacoSunSpecModbusRtuConnection::totalEnergyProducedChanged, this, [this, thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = (totalEnergyProduced * qPow(10, m_scalefactors.value(thing).energySf)) / 1000;
            qCDebug(dcKacoSunSpec()) << "Inverter total energy produced changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(kacosunspecInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });

        connect(connection, &KacoSunSpecModbusRtuConnection::operatingStateChanged, this, [this, thing](KacoSunSpecModbusRtuConnection::OperatingState operatingState){
            qCDebug(dcKacoSunSpec()) << "Inverter operating state recieved" << operatingState;
            setOperatingState(thing, operatingState);
        });


        // FIXME: make async and check if this is really a kaco sunspec
        //
        //
        m_rtuConnections.insert(thing, connection);
        ScaleFactors scalefactors{};
        m_scalefactors.insert(thing, scalefactors);

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == kaconh3ThingClassId) {
        qCDebug(dcKacoSunSpec()) << "Setting up NH3";

        // Handle reconfigure
        if (m_nh3Connections.contains(thing)) {
            qCDebug(dcKacoSunSpec()) << "Already have a Kaco SunSpec connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_nh3Connections.take(thing);
        } else {
            qCDebug(dcKacoSunSpec()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(kaconh3ThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcKacoSunSpec()) << "Failed to set up Kaco SunSpec inverter because the MAC address is not valid:" << thing->paramValue(kaconh3ThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Clean up in case the setup gets aborted
            if (m_monitors.contains(thing)) {
                qCDebug(dcKacoSunSpec()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        uint port = thing->paramValue(kaconh3ThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(kaconh3ThingSlaveIdParamTypeId).toUInt();

        KacoNH3ModbusTcpConnection *connection = new KacoNH3ModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        connection->setTimeout(4500);
        connect(info, &ThingSetupInfo::aborted, connection, &KacoNH3ModbusTcpConnection::deleteLater);

        connect(connection, &KacoNH3ModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue("connected", success);

            thing->setStateValue("deviceModel", connection->deviceModel());
            thing->setStateValue("firmwareVersion", connection->firmware());
            thing->setStateValue("serialNumber", connection->serialnumber());

            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", success);
            }

            if (!success) {
                // Try once to reconnect the device
                connection->reconnectDevice();
            } else {
                qCInfo(dcKacoSunSpec()) << "Connection initialized successfully for" << thing;
                connection->update();
            }
        });

        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcKacoSunSpec()) << "Network device monitor reachable changed for" << thing->name() << reachable;
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

        connect(connection, &KacoNH3ModbusTcpConnection::reachableChanged, thing, [this, thing, connection](bool reachable){
            qCInfo(dcKacoSunSpec()) << "Reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                connection->initialize();
            } else {
                thing->setStateValue("connected", false);
                thing->setStateValue(kaconh3CurrentPowerStateTypeId, 0);

                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }

                Thing *child = getMeterThing(thing);
                if (child) {
                    child->setStateValue(kacoNh3MeterCurrentPowerStateTypeId, 0);
                }

                child = getBatteryThing(thing);
                if (child) {
                    child->setStateValue(kacoNh3BatteryCurrentPowerStateTypeId, 0);
                }
            }
        });

        m_nh3Connections.insert(thing, connection);

        if (monitor->reachable())
            connection->connectDevice();

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == kacoNh3MeterThingClassId) {
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (!connectionThing) {
            qCWarning(dcKacoSunSpec()) << "Failed to set up Kaco energy meter because the parent thing with ID" << thing->parentId().toString() << "could not be found.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        auto connection = m_nh3Connections.value(connectionThing);
        if (!connection) {
            qCWarning(dcKacoSunSpec()) << "Failed to set up Kaco energy meter because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated
        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == kacoNh3BatteryThingClassId) {
        // Get the parent thing and the associated connection
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (!connectionThing) {
            qCWarning(dcKacoSunSpec()) << "Failed to set up Kaco battery because the parent thing with ID" << thing->parentId().toString() << "could not be found.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        auto connection = m_nh3Connections.value(connectionThing);
        if (!connection) {
            qCWarning(dcKacoSunSpec()) << "Failed to set up Kaco battery because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginKacoSunSpec::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == kacosunspecInverterTCPThingClassId || 
        thing->thingClassId() == kacosunspecInverterRTUThingClassId ||
        thing->thingClassId() == kaconh3ThingClassId) {
        if (!m_pluginTimer) {
            qCWarning(dcKacoSunSpec()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(KacoSunSpecModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }

                foreach(KacoNH3ModbusTcpConnection *connection, m_nh3Connections) {
                    qCWarning(dcKacoSunSpec()) << "NH3 connected for upate?";
                    if (connection->connected()) {
                        qCWarning(dcKacoSunSpec()) << "Updating NH3";
                        connection->update();
                    }
                }

                foreach(KacoSunSpecModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }

    if (thing->thingClassId() == kacoNh3MeterThingClassId ||
        thing->thingClassId() == kacoNh3BatteryThingClassId) {
        
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (connectionThing) {
            thing->setStateValue("connected", connectionThing->stateValue("connected"));
        }
    }
}

void IntegrationPluginKacoSunSpec::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == kacosunspecInverterTCPThingClassId && m_tcpConnections.contains(thing)) {
        qCWarning(dcKacoSunSpec()) << "Removing tcp connection";
        auto connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (thing->thingClassId() == kaconh3ThingClassId &&m_nh3Connections.contains(thing)) {
        qCWarning(dcKacoSunSpec()) << "Removing nh3 connection";
        auto connection = m_nh3Connections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if ((thing->thingClassId() == kacosunspecInverterTCPThingClassId ||
         thing->thingClassId() == kacosunspecInverterRTUThingClassId) && m_scalefactors.contains(thing)) {
        qCWarning(dcKacoSunSpec()) << "Removing scale factors";
        m_scalefactors.remove(thing);
    }

    if (thing->thingClassId() == kacosunspecInverterRTUThingClassId && m_rtuConnections.contains(thing)) {
        qCWarning(dcKacoSunSpec()) << "Removing rtu connection";
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (m_monitors.contains(thing)) {
        qCWarning(dcKacoSunSpec()) << "Removing monitor";
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        qCWarning(dcKacoSunSpec()) << "Deleting plugin timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        qCWarning(dcKacoSunSpec()) << "Deleting plugin timer";
        m_pluginTimer = nullptr;
        qCWarning(dcKacoSunSpec()) << "Deleting plugin timer";
    }
}

void IntegrationPluginKacoSunSpec::setOperatingState(Thing *thing, KacoSunSpecModbusTcpConnection::OperatingState state)
{
    switch (state) {
        case KacoSunSpecModbusTcpConnection::OperatingStateOff:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Off");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateSleeping:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Sleeping");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateStarting:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Starting");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateMppt:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "MPPT");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateThrottled:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Throttled");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateShuttingDown:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "ShuttingDown");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateFault:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Fault");
            break;
        case KacoSunSpecModbusTcpConnection::OperatingStateStandby:
            thing->setStateValue(kacosunspecInverterTCPOperatingStateStateTypeId, "Standby");
            break;
    }
}


void IntegrationPluginKacoSunSpec::setOperatingState(Thing *thing, KacoSunSpecModbusRtuConnection::OperatingState state)
{
    switch (state) {
        case KacoSunSpecModbusRtuConnection::OperatingStateOff:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Off");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateSleeping:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Sleeping");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateStarting:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Starting");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateMppt:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "MPPT");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateThrottled:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Throttled");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateShuttingDown:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "ShuttingDown");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateFault:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Fault");
            break;
        case KacoSunSpecModbusRtuConnection::OperatingStateStandby:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Standby");
            break;
    }
}

void IntegrationPluginKacoSunSpec::setOperatingState(Thing *thing, KacoNH3ModbusTcpConnection::OperatingState state)
{
    switch (state) {
        case KacoNH3ModbusTcpConnection::OperatingStateOff:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "Off");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateSleeping:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "Sleeping");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateStarting:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "Starting");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateMppt:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "MPPT");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateThrottled:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "Throttled");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateShuttingDown:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "ShuttingDown");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateFault:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "Fault");
            break;
        case KacoNH3ModbusTcpConnection::OperatingStateStandby:
            thing->setStateValue(kaconh3OperatingStateStateTypeId, "Standby");
            break;
    }
}

void IntegrationPluginKacoSunSpec::setChargingState(Thing *thing, KacoNH3ModbusTcpConnection::ChargeStatus state)
{
    switch (state) {
        case KacoNH3ModbusTcpConnection::ChargeStatusOff:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "idle");
            break;
        case KacoNH3ModbusTcpConnection::ChargeStatusEmpty:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "idle");
            break;
        case KacoNH3ModbusTcpConnection::ChargeStatusDischarging:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "charging");
            break;
        case KacoNH3ModbusTcpConnection::ChargeStatusCharging:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "charging");
            break;
        case KacoNH3ModbusTcpConnection::ChargeStatusFull:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "idle");
            break;
        case KacoNH3ModbusTcpConnection::ChargeStatusHolding:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "idle");
            break;
        case KacoNH3ModbusTcpConnection::ChargeStatusTesting:
            thing->setStateValue(kacoNh3BatteryChargingStateStateTypeId, "idle");
            break;
    }
}

void IntegrationPluginKacoSunSpec::setBatteryState(Thing *thing, KacoNH3ModbusTcpConnection::BatteryStatus state)
{
    switch (state) {
        case KacoNH3ModbusTcpConnection::BatteryStatusDisconnected:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "Disconnected");
            break;
        case KacoNH3ModbusTcpConnection::BatteryStatusInitializing:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "Initializing");
            break;
        case KacoNH3ModbusTcpConnection::BatteryStatusConnected:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "Connected");
            break;
        case KacoNH3ModbusTcpConnection::BatteryStatusStandby:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "Standby");
            break;
        case KacoNH3ModbusTcpConnection::BatteryStatusSocProtection:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "SoC Protection");
            break;
        case KacoNH3ModbusTcpConnection::BatteryStatusSuspending:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "Suspending");
            break;
        case KacoNH3ModbusTcpConnection::BatteryStatusFault:
            thing->setStateValue(kacoNh3BatteryBatteryStateStateTypeId, "Fault");
            break;
    }
}

Thing *IntegrationPluginKacoSunSpec::getMeterThing(Thing *parentThing)
{
    Things meterThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(kacoNh3MeterThingClassId);
    if (meterThings.isEmpty())
        return nullptr;

    return meterThings.first();
}

Thing *IntegrationPluginKacoSunSpec::getBatteryThing(Thing *parentThing)
{
    Things batteryThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(kacoNh3BatteryThingClassId);
    if (batteryThings.isEmpty())
        return nullptr;

    return batteryThings.first();
}
