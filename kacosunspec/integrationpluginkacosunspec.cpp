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
    }

    if (thing->thingClassId() == kacosunspecInverterRTUThingClassId) {

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
                qCWarning(dcKacoSunSpec()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
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
    }
}

void IntegrationPluginKacoSunSpec::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == kacosunspecInverterTCPThingClassId || thing->thingClassId() == kacosunspecInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcKacoSunSpec()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(KacoSunSpecModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->connected()) {
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
}

void IntegrationPluginKacoSunSpec::thingRemoved(Thing *thing)
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
