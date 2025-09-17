/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#include "integrationpluginsunspec.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>

#include <sunspecmodel.h>
#include <models/sunspecmodelfactory.h>
#include <models/sunspeccommonmodel.h>

// Inverters
#include <models/sunspecinvertersinglephasemodel.h>
#include <models/sunspecinvertersinglephasefloatmodel.h>
#include <models/sunspecinvertersplitphasemodel.h>
#include <models/sunspecinvertersplitphasefloatmodel.h>
#include <models/sunspecinverterthreephasemodel.h>
#include <models/sunspecinverterthreephasefloatmodel.h>

// Meters
#include <models/sunspecmeterthreephasemodel.h>
#include <models/sunspecmetersinglephasemodel.h>
#include <models/sunspecmetersinglephasefloatmodel.h>
#include <models/sunspecmetersplitsinglephaseabnmodel.h>
#include <models/sunspecmetersplitsinglephasefloatmodel.h>
#include <models/sunspecmeterthreephasewyeconnectmodel.h>
#include <models/sunspecmeterthreephasedeltaconnectmodel.h>
#include <models/sunspecdeltaconnectthreephaseabcmetermodel.h>

#include <models/sunspecstoragemodel.h>
#include <models/sunspecsettingsmodel.h>
#include <models/sunspeccontrolsmodel.h>
#include <models/sunspecnameplatemodel.h>
#include <models/sunspecmpptmodel.h>

#include "sunspecdiscovery.h"
#include "solaredgebattery.h"

#include <QHostAddress>

QHash<ThingClassId, ParamTypeId> IntegrationPluginSunSpec::enableExportLimitParamTypeId =
        QHash<ThingClassId, ParamTypeId>{};
QHash<ThingClassId, ParamTypeId> IntegrationPluginSunSpec::exportLimitParamTypeId =
        QHash<ThingClassId, ParamTypeId>{};

IntegrationPluginSunSpec::IntegrationPluginSunSpec()
{
    enableExportLimitParamTypeId[sunspecLimitableSinglePhaseInverterThingClassId] =
            sunspecLimitableSinglePhaseInverterEnableExportLimitActionEnableExportLimitParamTypeId;
    enableExportLimitParamTypeId[sunspecLimitableSplitPhaseInverterThingClassId] =
            sunspecLimitableSplitPhaseInverterEnableExportLimitActionEnableExportLimitParamTypeId;
    enableExportLimitParamTypeId[sunspecLimitableThreePhaseInverterThingClassId] =
            sunspecLimitableThreePhaseInverterEnableExportLimitActionEnableExportLimitParamTypeId;

    exportLimitParamTypeId[sunspecLimitableSinglePhaseInverterThingClassId] =
            sunspecLimitableSinglePhaseInverterExportLimitActionExportLimitParamTypeId;
    exportLimitParamTypeId[sunspecLimitableSplitPhaseInverterThingClassId] =
            sunspecLimitableSplitPhaseInverterExportLimitActionExportLimitParamTypeId;
    exportLimitParamTypeId[sunspecLimitableThreePhaseInverterThingClassId] =
            sunspecLimitableThreePhaseInverterExportLimitActionExportLimitParamTypeId;
}

void IntegrationPluginSunSpec::init()
{
    // SunSpec connection params
    m_connectionPortParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingPortParamTypeId);
    m_connectionPortParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingPortParamTypeId);

    m_connectionMacAddressParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingMacAddressParamTypeId);
    m_connectionMacAddressParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingMacAddressParamTypeId);

    m_connectionSlaveIdParamTypeIds.insert(sunspecConnectionThingClassId, sunspecConnectionThingSlaveIdParamTypeId);
    m_connectionSlaveIdParamTypeIds.insert(solarEdgeConnectionThingClassId, solarEdgeConnectionThingSlaveIdParamTypeId);

    // Params for sunspec things
    m_modelIdParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingModelIdParamTypeId);
    m_modelIdParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingModelIdParamTypeId);

    m_modbusAddressParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingModbusAddressParamTypeId);
    m_modbusAddressParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingModbusAddressParamTypeId);

    m_manufacturerParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingManufacturerParamTypeId);
    m_manufacturerParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingManufacturerParamTypeId);

    m_deviceModelParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingDeviceModelParamTypeId);
    m_deviceModelParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingDeviceModelParamTypeId);

    m_serialNumberParamTypeIds.insert(solarEdgeBatteryThingClassId, solarEdgeBatteryThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSinglePhaseInverterThingClassId, sunspecSinglePhaseInverterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSplitPhaseInverterThingClassId, sunspecSplitPhaseInverterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecThreePhaseInverterThingClassId, sunspecThreePhaseInverterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecStorageThingClassId, sunspecStorageThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSinglePhaseMeterThingClassId, sunspecSinglePhaseMeterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecSplitPhaseMeterThingClassId, sunspecSplitPhaseMeterThingSerialNumberParamTypeId);
    m_serialNumberParamTypeIds.insert(sunspecThreePhaseMeterThingClassId, sunspecThreePhaseMeterThingSerialNumberParamTypeId);
}

void IntegrationPluginSunSpec::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcSunSpec()) << "Failed to discover network devices. The network device discovery is not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The discovery is not available."));
        return;
    }

    QList<quint16> slaveIds = {1, 2};
    SunSpecDataPoint::ByteOrder byteOrder = SunSpecDataPoint::ByteOrderLittleEndian;
    if (info->thingClassId() == solarEdgeConnectionThingClassId) {
        byteOrder = SunSpecDataPoint::ByteOrderBigEndian;
    }

    SunSpecDiscovery *discovery = new SunSpecDiscovery(hardwareManager()->networkDeviceDiscovery(), slaveIds, byteOrder, info);
    // Note: we could add here more
    connect(discovery, &SunSpecDiscovery::discoveryFinished, info, [=](){
        foreach (const SunSpecDiscovery::Result &result, discovery->results()) {

            // Extract the manufacturer: we pick the first manufacturer name of the first common model having a manufacturer name for now
            QString manufacturer;
            if (!result.modelManufacturers.isEmpty())
                manufacturer = result.modelManufacturers.first();

            qCDebug(dcSunSpec()) << "Found manufacturers on" << result.networkDeviceInfo << result.port;
            qCDebug(dcSunSpec()) << "Manufacturers:" << result.modelManufacturers;
            qCDebug(dcSunSpec()) << "Picking manufacturer for evaluation:" << manufacturer;

            // Filter for solar edge if we got one here
            if (info->thingClassId() == solarEdgeConnectionThingClassId) {
                if (!hasManufacturer(result.modelManufacturers, "solaredge") && !hasManufacturer(result.modelManufacturers, "solar edge")) {
                    // Solar edge...we must have the manufacturer in one common model
                    continue;
                } else {
                    manufacturer = "SolarEdge";
                }
            } else if (info->thingClassId() == sunspecConnectionThingClassId) {
                // There are some issues regarding the sunspec implementation of kostal.
                // Full support of meter, inverter and storage will be provided in the kostal plugin which makes
                // use of the native modbus communication from kostal.
                if (hasManufacturer(result.modelManufacturers, "kostal")) {
                    continue;
                }
            }

            QString title;
            if (!manufacturer.isEmpty()) {
                title = manufacturer + " ";
            }
            title.append("SunSpec connection");

            QString description;
            if (result.networkDeviceInfo.macAddressManufacturer().isEmpty()) {
                description = result.networkDeviceInfo.macAddress();
            } else {
                description = result.networkDeviceInfo.macAddress() + " (" + result.networkDeviceInfo.macAddressManufacturer() + ")";
            }

            ThingDescriptor descriptor(info->thingClassId(), title, description);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(m_connectionMacAddressParamTypeIds.value(info->thingClassId()), result.networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcSunSpec()) << "This thing already exists in the system." << existingThings.first() << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(m_connectionPortParamTypeIds.value(info->thingClassId()), result.port);
            params << Param(m_connectionMacAddressParamTypeIds.value(info->thingClassId()), result.networkDeviceInfo.macAddress());
            params << Param(m_connectionSlaveIdParamTypeIds.value(info->thingClassId()), result.slaveId);
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        // Discovery done
        info->finish(Thing::ThingErrorNoError);
    });

    discovery->startDiscovery();
}

void IntegrationPluginSunSpec::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSunSpec()) << "Setup thing" << thing;
    qCDebug(dcSunSpec()) << thing->params();

    if (thing->thingClassId() == sunspecConnectionThingClassId || thing->thingClassId() == solarEdgeConnectionThingClassId) {

        // Handle reconfigure
        if (m_sunSpecConnections.contains(thing->id())) {
            qCDebug(dcSunSpec()) << "Reconfiguring existing thing" << thing->name();
            m_sunSpecConnections.take(thing->id())->deleteLater();

            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }

        MacAddress macAddress = MacAddress(thing->paramValue(m_connectionMacAddressParamTypeIds.value(thing->thingClassId())).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcSunSpec()) << "The configured mac address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the connection."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        QHostAddress address = monitor->networkDeviceInfo().address();
        if (address.isNull() && info->isInitialSetup()) {
            qCWarning(dcSunSpec()) << "Cannot set up thing. The host address is not known and this is an initial setup";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The host address is not known yet. Trying later again."));
            return;
        }

        // Clean up in case the setup gets aborted
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcSunSpec()) << "Unregister monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        SunSpecConnection *connection = createConnection(info->thing());
        connect(info, &ThingSetupInfo::aborted, connection, &SunSpecConnection::deleteLater);

        // If this is the first setup, we must connect successfully fo finishing the setup, otherwise we let the monitor do his work
        if (info->isInitialSetup()) {

            // Only during setup
            connect(connection, &SunSpecConnection::connectedChanged, info, [this, connection, info] (bool connected) {
                if (connected) {
                    connect(connection, &SunSpecConnection::discoveryFinished, info, [this, connection, info] (bool success) {
                        if (success) {
                            qCDebug(dcSunSpec()) << "Discovery finished successfully during setup of" << connection << ". Found SunSpec data on base register" << connection->baseRegister();
                            m_sunSpecConnections.insert(info->thing()->id(), connection);
                            info->finish(Thing::ThingErrorNoError);
                            processDiscoveryResult(info->thing(), connection);
                        } else {
                            qCWarning(dcSunSpec()) << "Discovery finished with errors during setup of" << connection;
                            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The SunSpec discovery finished with errors. Please make sure this is a SunSpec device."));
                            connection->deleteLater();
                        }
                    });

                    // Perform initial discovery, finish if a valid base register has been found
                    connection->startDiscovery();
                } else {
                    info->finish(Thing::ThingErrorHardwareNotAvailable);
                    connection->deleteLater();
                }
            });

            connect(info, &ThingSetupInfo::aborted, connection, &SunSpecConnection::deleteLater);

            if (!connection->connectDevice()) {
                qCWarning(dcSunSpec()) << "Error connecting to SunSpec device" << thing;
                info->finish(Thing::ThingErrorHardwareNotAvailable);
                connection->deleteLater();
                return;
            }

        } else {
            m_sunSpecConnections.insert(thing->id(), connection);

            // Finish the setup in any case, not the initial setup and the monitor takes care about connecting
            info->finish(Thing::ThingErrorNoError);

            if (monitor->reachable()) {
                // Thing already reachable...let's continue with the setup
                connection->connectDevice();
            } else {
                // Wait for the monitor to be ready
                qCDebug(dcSunSpec()) << "Waiting for the network monitor to get reachable before continue to set up the connection" << thing->name() << address.toString() << "...";
            }
        }

    } else if (thing->thingClassId() == sunspecThreePhaseInverterThingClassId
               || thing->thingClassId() == sunspecSplitPhaseInverterThingClassId
               || thing->thingClassId() == sunspecSinglePhaseInverterThingClassId ) {

        Thing *thing = info->thing();
        uint modelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toInt();
        int modbusStartRegister = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toInt();
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
        if (connection) {
            // Get the model from the connection if already available
            foreach (SunSpecModel *model, connection->models()) {
                if (model->modelId() == modelId && model->modbusStartRegister() == modbusStartRegister) {
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onInverterBlockUpdated);
                    m_sunSpecInverters.insert(thing, model);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << thing;
                }
            }
        }
        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId
               || thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId
               || thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId) {
        Thing *thing = info->thing();
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
        if (connection) {
            initializeLimitableInverterModels(thing, connection);
        } else {
            qCWarning(dcSunSpec()) << "SunSpec connection not available for thing:" << thing;
        }
        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == sunspecThreePhaseMeterThingClassId
               || thing->thingClassId() == sunspecSplitPhaseMeterThingClassId
               || thing->thingClassId() == sunspecSinglePhaseMeterThingClassId ) {

        Thing *thing = info->thing();
        uint modelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toInt();
        int modbusStartRegister = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toInt();
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
        if (connection) {
            // Get the model from the connection
            foreach (SunSpecModel *model, connection->models()) {
                if (model->modelId() == modelId && model->modbusStartRegister() == modbusStartRegister) {
                    m_sunSpecMeters.insert(thing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onMeterBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << thing;
                }
            }
        } else {
            // Note: will be initialized once the connection is available and discovered.
            qCDebug(dcSunSpec()) << "Model not available yet for" << thing;
        }

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == sunspecStorageThingClassId) {
        Thing *thing = info->thing();
        uint modelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toInt();
        int modbusStartRegister = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toInt();
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
        if (connection) {
            // Get the model from the connection
            foreach (SunSpecModel *model, connection->models()) {
                if (model->modelId() == modelId && model->modbusStartRegister() == modbusStartRegister) {
                    m_sunSpecStorages.insert(thing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onStorageBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << thing;
                }
            }
        } else {
            // Note: will be initialized once the connection is available and discovered.
            qCDebug(dcSunSpec()) << "Model not available yet for" << thing;
        }

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == froniusControllableStorageThingClassId) {
        Thing *thing = info->thing();
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
        if (connection) {
            initializeFroniusControllableStorageModels(thing, connection);
        } else {
            qCWarning(dcSunSpec()) << "SunSpec connection not available for thing:" << thing;
        }
        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == solarEdgeBatteryThingClassId) {

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing->setupStatus() == Thing::ThingSetupStatusComplete) {
            setupSolarEdgeBattery(info);
        } else {
            connect(parentThing, &Thing::setupStatusChanged, info, [this, info] {
                setupSolarEdgeBattery(info);
            });
        }
    } else {
        Q_ASSERT_X(false, "setupThing", QString("Unhandled thingClassId: %1").arg(info->thing()->thingClassId().toString()).toUtf8());
    }
}

void IntegrationPluginSunSpec::postSetupThing(Thing *thing)
{
    qCDebug(dcSunSpec()) << "Post setup thing" << thing->name();

    if (thing->thingClassId() == solarEdgeConnectionThingClassId) {
        SunSpecConnection *connection = m_sunSpecConnections.value(thing->id());
        if (connection) {
            searchSolarEdgeBatteries(connection);
        }
    }

    // Create the refresh timer if not already set up
    if (!m_refreshTimer) {
        qCDebug(dcSunSpec()) << "Starting refresh timer";
        int refreshTime = configValue(sunSpecPluginUpdateIntervalParamTypeId).toInt();
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(refreshTime);
        connect(m_refreshTimer, &PluginTimer::timeout, this, &IntegrationPluginSunSpec::onRefreshTimer);
    }
}

void IntegrationPluginSunSpec::thingRemoved(Thing *thing)
{
    qCDebug(dcSunSpec()) << "Thing removed" << thing->name();

    if (m_sunSpecConnections.contains(thing->id())) {
        m_sunSpecConnections.take(thing->id())->deleteLater();
    } else if (m_sunSpecThings.contains(thing)) {
        m_sunSpecThings.take(thing)->deleteLater();
    } else if (m_sunSpecInverters.contains(thing)) {
        m_sunSpecInverters.remove(thing);
    } else if (m_sunSpecMeters.contains(thing)) {
        m_sunSpecMeters.remove(thing);
    } else if (m_sunSpecStorages.contains(thing)) {
        m_sunSpecStorages.remove(thing);
    } else if (m_sunSpecSettings.contains(thing)) {
        m_sunSpecSettings.remove(thing);
    } else if (m_sunSpecControls.contains(thing)) {
        m_sunSpecControls.remove(thing);
    } else if (m_sunSpecNameplates.contains(thing)) {
        m_sunSpecNameplates.remove(thing);
    } else if (m_sunSpecMppts.contains(thing)) {
        m_sunSpecMppts.remove(thing);
    } else {
        Q_ASSERT_X(false, "thingRemoved", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (myThings().isEmpty()) {
        qCDebug(dcSunSpec()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

void IntegrationPluginSunSpec::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == sunspecStorageThingClassId) {
        SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
        if (!storage) {
            qWarning(dcSunSpec()) << "Could not find sunspec model instance for thing" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!thing->stateValue(sunspecStorageConnectedStateTypeId).toBool()) {
            qWarning(dcSunSpec()) << "Could not execute action for" << thing << "because the SunSpec connection is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The SunSpec connection is not connected."));
            return;
        }

        if (action.actionTypeId() == sunspecStorageGridChargingActionTypeId) {
            bool gridCharging = action.param(sunspecStorageGridChargingActionGridChargingParamTypeId).value().toBool();
            QModbusReply *reply = storage->setChaGriSet(gridCharging ? SunSpecStorageModel::ChagrisetGrid : SunSpecStorageModel::ChagrisetPv);
            if (!reply) {
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply]{
                if (reply->error() != QModbusDevice::NoError) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else if (action.actionTypeId() == sunspecStorageEnableChargingActionTypeId || action.actionTypeId() == sunspecStorageEnableDischargingActionTypeId) {
            SunSpecStorageModel::Storctl_modFlags controlModeFlags;
            if (action.param(sunspecStorageEnableChargingActionEnableChargingParamTypeId).value().toBool())
                controlModeFlags.setFlag(SunSpecStorageModel::Storctl_modCharge);

            if (thing->stateValue(sunspecStorageEnableDischargingStateTypeId).toBool())
                controlModeFlags.setFlag(SunSpecStorageModel::Storctl_modDiScharge);

            QModbusReply *reply = storage->setStorCtlMod(controlModeFlags);
            if (!reply) {
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply]{
                if (reply->error() != QModbusDevice::NoError) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else if (action.actionTypeId() == sunspecStorageChargingRateActionTypeId) {
            QModbusReply *reply = storage->setInWRte(action.param(sunspecStorageChargingRateActionChargingRateParamTypeId).value().toInt());
            if (!reply) {
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply]{
                if (reply->error() != QModbusDevice::NoError) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else if (action.actionTypeId() == sunspecStorageDischargingRateActionTypeId) {
            QModbusReply *reply = storage->setOutWRte(action.param(sunspecStorageDischargingRateActionDischargingRateParamTypeId).value().toInt());
            if (!reply) {
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply]{
                if (reply->error() != QModbusDevice::NoError) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(action.actionTypeId().toString()).toUtf8());
        }

    } else if (thing->thingClassId() == solarEdgeBatteryThingClassId) {
        SolarEdgeBattery *battery = qobject_cast<SolarEdgeBattery *>(m_sunSpecThings.value(thing));
        if (!battery) {
            qWarning(dcSunSpec()) << "Could not find SolarEdgeBattery instance for thing" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solarEdgeBatteryEnableForcePowerActionTypeId) {
            bool enableForcePower = action.param(solarEdgeBatteryEnableForcePowerActionTypeId).value().toBool();
            battery->setForcePowerEnabled(enableForcePower);
            info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == solarEdgeBatteryForcePowerActionTypeId) {
            int forcePower = action.param(solarEdgeBatteryForcePowerActionTypeId).value().toInt();
            // uint forcePowerTimeout = action.param(solarEdgeBatteryForcePowerTimeoutActionTypeId).value().toUInt();
            battery->setChargeDischargePower(forcePower, 900);
            info->finish(Thing::ThingErrorNoError);
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(action.actionTypeId().toString()).toUtf8());
        }
    } else if (thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId ||
               thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId ||
               thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
        if (action.actionTypeId() == sunspecLimitableSinglePhaseInverterEnableExportLimitActionTypeId ||
                action.actionTypeId() == sunspecLimitableSplitPhaseInverterEnableExportLimitActionTypeId ||
                action.actionTypeId() == sunspecLimitableThreePhaseInverterEnableExportLimitActionTypeId) {
            qCDebug(dcSunSpec()) << "EnableExportLimit action triggered";
            const auto enableLimit =
                    action.paramValue(enableExportLimitParamTypeId.value(thing->thingClassId())).toBool();
            setEnableExportLimit(thing, enableLimit, info);
        } else if (action.actionTypeId() == sunspecLimitableSinglePhaseInverterExportLimitActionTypeId ||
                   action.actionTypeId() == sunspecLimitableSplitPhaseInverterExportLimitActionTypeId ||
                   action.actionTypeId() == sunspecLimitableThreePhaseInverterExportLimitActionTypeId) {
            qCDebug(dcSunSpec()) << "SetExportLimit action triggered";
            const auto maxPowerOutput = thing->stateValue("maxPowerOutput").toFloat();
            if (qFuzzyIsNull(maxPowerOutput)) {
                qCWarning(dcSunSpec()) << "WMax value not available for thing" << thing->name();
                info->finish(Thing::ThingErrorInvalidParameter);
                return;
            }
            const auto limitToSet =
                    action.paramValue(exportLimitParamTypeId.value(thing->thingClassId())).toDouble() / maxPowerOutput * 100.f;
            const auto limitEnabled =
                    thing->stateValue("enableExportLimit").toBool();
            if (limitEnabled) {
                const auto reply = setExportLimit(thing, limitToSet, nullptr); // Use nullptr for info to not yet finish action.
                if (!reply) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                connect(reply, &QModbusReply::finished, reply, [this, thing, info, reply]() {
                    if (reply->error() != QModbusDevice::NoError) {
                        info->finish(Thing::ThingErrorHardwareFailure);
                        return;
                    }
                    // Need to set WMaxLimPct_Ena again to enable new export limit.
                    setEnableExportLimit(thing, true, info);
                });
            } else {
                setExportLimit(thing, limitToSet, info);
            }
        } else {
            qCWarning(dcSunSpec()) << "Unknown action type:" << action.actionTypeId();
            info->finish(Thing::ThingErrorActionTypeNotFound,
                         QT_TR_NOOP("Unknown action type"));
        }
        // #TODO add limitgridexport interface in plugin json again
    } else if (thing->thingClassId() == froniusControllableStorageThingClassId) {
        if (action.actionTypeId() == froniusControllableStorageEnableForcePowerActionTypeId) {
            const auto enableForcePower =
                    action.paramValue(froniusControllableStorageEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
            if (enableForcePower) {
                const auto reply = setEnableForcePower(thing, enableForcePower, nullptr); // Use nullptr for info to not yet finish action.
                if (!reply) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                connect(reply, &QModbusReply::finished, reply, [this, thing, info, reply]() {
                    if (reply->error() != QModbusDevice::NoError) {
                        info->finish(Thing::ThingErrorHardwareFailure);
                        return;
                    }
                    const auto forcePower = thing->stateValue(froniusControllableStorageForcePowerStateTypeId).toDouble();
                    setForcePower(thing, forcePower, info);
                });
            } else {
                setEnableForcePower(thing, enableForcePower, info);
            }
            connect(info, &ThingActionInfo::finished, info, [thing, info, enableForcePower]() {
                if (info->status() == Thing::ThingErrorNoError) {
                    thing->setStateValue(froniusControllableStorageEnableForcePowerStateStateTypeId, enableForcePower);
                    thing->setStateValue(froniusControllableStorageEnableForcePowerStateTypeId, enableForcePower);
                }
            });
        } else if (action.actionTypeId() == froniusControllableStorageForcePowerActionTypeId) {
            const auto forcePower =
                    action.paramValue(froniusControllableStorageForcePowerActionForcePowerParamTypeId).toDouble();
            setForcePower(thing, forcePower, info);
            connect(info, &ThingActionInfo::finished, info, [thing, info, forcePower]() {
                if (info->status() == Thing::ThingErrorNoError) {
                    thing->setStateValue(froniusControllableStorageForcePowerStateTypeId, forcePower);
                }
            });
        } else if (action.actionTypeId() == froniusControllableStorageMaxChargingCurrentActionTypeId) {
            qCInfo(dcSunSpec()) << "Ignoring unsupported action" << action.actionTypeId();
            info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == froniusControllableStorageEnableMaxChargingCurrentActionTypeId) {
            // #TODO Use enableMaxCharging also for setMaxSoC?
            qCInfo(dcSunSpec()) << "Ignoring unsupported action" << action.actionTypeId();
            info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == froniusControllableStorageSetMaxSoCActionTypeId) {
            const auto maxSoC =
                    action.paramValue(froniusControllableStorageSetMaxSoCActionSetMaxSoCParamTypeId).toUInt();
            const auto currentSoC = thing->stateValue(froniusControllableStorageBatteryLevelStateTypeId).toUInt();
            if (currentSoC < maxSoC) {
                setChargingAllowed(thing, true, info);
            } else {
                setChargingAllowed(thing, false, info);
            }
            connect(info, &ThingActionInfo::finished, info, [thing, info, maxSoC]() {
                if (info->status() == Thing::ThingErrorNoError) {
                    thing->setStateValue(froniusControllableStorageSetMaxSoCStateTypeId, maxSoC);
                }
            });
        } else if (action.actionTypeId() == froniusControllableStorageInWRteSetpointActionTypeId) {
            const auto inWRteToSet =
                    action.paramValue(froniusControllableStorageInWRteSetpointActionInWRteSetpointParamTypeId).toDouble();
            SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
            const auto reply = storage->setInWRte(inWRteToSet);
            if (!reply) {
                qWarning(dcSunSpec()) << "Unable to set InWRte for thing" << thing->name();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, thing]() {
                if (reply->error() != QModbusDevice::NoError) {
                    qWarning(dcSunSpec()) << "Unable to set InWRte for thing" << thing->name();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else if (action.actionTypeId() == froniusControllableStorageOutWRteSetpointActionTypeId) {
            const auto outWRteToSet =
                    action.paramValue(froniusControllableStorageOutWRteSetpointActionOutWRteSetpointParamTypeId).toDouble();
            SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
            const auto reply = storage->setOutWRte(outWRteToSet);
            if (!reply) {
                qWarning(dcSunSpec()) << "Unable to set OutWRte for thing" << thing->name();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, thing]() {
                if (reply->error() != QModbusDevice::NoError) {
                    qWarning(dcSunSpec()) << "Unable to set OutWRte for thing" << thing->name();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else if (action.actionTypeId() == froniusControllableStorageChargeLimitActionTypeId) {
            const auto enableChargeLimit =
                    action.paramValue(froniusControllableStorageChargeLimitActionChargeLimitParamTypeId).toBool();
            const auto dischargeLimitEnabled =
                    thing->stateValue(froniusControllableStorageDischargeLimitStateTypeId).toBool();
            auto storCtlModToSet = SunSpecStorageModel::Storctl_modFlags{};
            storCtlModToSet.setFlag(SunSpecStorageModel::Storctl_modCharge, enableChargeLimit);
            storCtlModToSet.setFlag(SunSpecStorageModel::Storctl_modDiScharge, dischargeLimitEnabled);
            SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
            const auto reply = storage->setStorCtlMod(storCtlModToSet);
            if (!reply) {
                qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, thing]() {
                if (reply->error() != QModbusDevice::NoError) {
                    qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else if (action.actionTypeId() == froniusControllableStorageDischargeLimitActionTypeId) {
            const auto enableDischargeLimit =
                    action.paramValue(froniusControllableStorageDischargeLimitActionDischargeLimitParamTypeId).toBool();
            const auto chargeLimitEnabled =
                    thing->stateValue(froniusControllableStorageChargeLimitStateTypeId).toBool();
            auto storCtlModToSet = SunSpecStorageModel::Storctl_modFlags{};
            storCtlModToSet.setFlag(SunSpecStorageModel::Storctl_modCharge, chargeLimitEnabled);
            storCtlModToSet.setFlag(SunSpecStorageModel::Storctl_modDiScharge, enableDischargeLimit);
            SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
            const auto reply = storage->setStorCtlMod(storCtlModToSet);
            if (!reply) {
                qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, thing]() {
                if (reply->error() != QModbusDevice::NoError) {
                    qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                info->finish(Thing::ThingErrorNoError);
            });
        } else {
            qCWarning(dcSunSpec()) << "Unknown action type:" << action.actionTypeId();
            info->finish(Thing::ThingErrorActionTypeNotFound,
                         QT_TR_NOOP("Unknown action type"));
        }
    } else {
        Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(info->thing()->thingClassId().toString()).toUtf8());
    }
}

Thing *IntegrationPluginSunSpec::getThingForSunSpecModel(uint modelId, uint modbusAddress, const ThingId &parentId)
{
    foreach (Thing *thing, myThings()) {
        if (!m_modelIdParamTypeIds.contains(thing->thingClassId()))
            continue;

        uint thingModelId = thing->paramValue(m_modelIdParamTypeIds.value(thing->thingClassId())).toUInt();
        uint thingModbusAddress = thing->paramValue(m_modbusAddressParamTypeIds.value(thing->thingClassId())).toUInt();
        if (thingModelId == modelId && thingModbusAddress == modbusAddress && thing->parentId() == parentId) {
            return thing;
        }
    }

    return nullptr;
}

void IntegrationPluginSunSpec::processDiscoveryResult(Thing *thing, SunSpecConnection *connection)
{
    qCInfo(dcSunSpec()) << "Processing discovery result from" << thing->name() << connection;

    auto modelsById = QMap<quint16, SunSpecModel *>{};
    foreach (SunSpecModel *model, connection->models()) {
        modelsById.insert(model->modelId(), model);
    }
    const auto isInverter =
            modelsById.contains(SunSpecModelFactory::ModelIdInverterSinglePhase) ||
            modelsById.contains(SunSpecModelFactory::ModelIdInverterSinglePhaseFloat) ||
            modelsById.contains(SunSpecModelFactory::ModelIdInverterSplitPhase) ||
            modelsById.contains(SunSpecModelFactory::ModelIdInverterSplitPhaseFloat) ||
            modelsById.contains(SunSpecModelFactory::ModelIdInverterThreePhase) ||
            modelsById.contains(SunSpecModelFactory::ModelIdInverterThreePhaseFloat);
    const auto isLimitableInverter = isInverter &&
            modelsById.contains(SunSpecModelFactory::ModelIdSettings) &&
            modelsById.contains(SunSpecModelFactory::ModelIdControls);
    if (isLimitableInverter) {
        auto limitableInverterAlreadyExists = false;
        foreach(Thing *childThing, myThings().filterByParentId(thing->id())) {
            if (childThing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId ||
                    childThing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId ||
                    childThing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
                qCDebug(dcSunSpec()) << "Limitable inverter already exists for this thing";
                initializeLimitableInverterModels(childThing, connection);
                limitableInverterAlreadyExists = true;
                break;
            }
        }
        if (!limitableInverterAlreadyExists) {
            if (modelsById.contains(SunSpecModelFactory::ModelIdInverterSinglePhase) ||
                    modelsById.contains(SunSpecModelFactory::ModelIdInverterSinglePhaseFloat)) {
                const auto model =
                        modelsById.contains(SunSpecModelFactory::ModelIdInverterSinglePhase) ?
                            modelsById.value(SunSpecModelFactory::ModelIdInverterSinglePhase) :
                            modelsById.value(SunSpecModelFactory::ModelIdInverterSinglePhaseFloat);
                ThingDescriptor descriptor(sunspecLimitableSinglePhaseInverterThingClassId);
                descriptor.setParentId(thing->id());
                const auto thingName = QT_TR_NOOP("Limitable Single Phase Inverter");
                QString finalThingName;
                if (model->commonModelInfo().manufacturerName.isEmpty()) {
                    finalThingName = thingName;
                } else {
                    finalThingName = model->commonModelInfo().manufacturerName + " " + thingName;
                }
                descriptor.setTitle(finalThingName);
                descriptor.setParams(ParamList{});
                qCDebug(dcSunSpec()) << "Auto appearing thing" << finalThingName;
                emit autoThingsAppeared({descriptor});
            } else if (modelsById.contains(SunSpecModelFactory::ModelIdInverterSplitPhase) ||
                       modelsById.contains(SunSpecModelFactory::ModelIdInverterSplitPhaseFloat)) {
                const auto model =
                        modelsById.contains(SunSpecModelFactory::ModelIdInverterSplitPhase) ?
                            modelsById.value(SunSpecModelFactory::ModelIdInverterSplitPhase) :
                            modelsById.value(SunSpecModelFactory::ModelIdInverterSplitPhaseFloat);
                ThingDescriptor descriptor(sunspecLimitableSplitPhaseInverterThingClassId);
                descriptor.setParentId(thing->id());
                const auto thingName = QT_TR_NOOP("Limitable Split Phase Inverter");
                QString finalThingName;
                if (model->commonModelInfo().manufacturerName.isEmpty()) {
                    finalThingName = thingName;
                } else {
                    finalThingName = model->commonModelInfo().manufacturerName + " " + thingName;
                }
                descriptor.setTitle(finalThingName);
                descriptor.setParams(ParamList{});
                qCDebug(dcSunSpec()) << "Auto appearing thing" << finalThingName;
                emit autoThingsAppeared({descriptor});
            } else {
                const auto model =
                        modelsById.contains(SunSpecModelFactory::ModelIdInverterThreePhase) ?
                            modelsById.value(SunSpecModelFactory::ModelIdInverterThreePhase) :
                            modelsById.value(SunSpecModelFactory::ModelIdInverterThreePhaseFloat);
                ThingDescriptor descriptor(sunspecLimitableThreePhaseInverterThingClassId);
                descriptor.setParentId(thing->id());
                const auto thingName = QT_TR_NOOP("Limitable Three Phase Inverter");
                QString finalThingName;
                if (model->commonModelInfo().manufacturerName.isEmpty()) {
                    finalThingName = thingName;
                } else {
                    finalThingName = model->commonModelInfo().manufacturerName + " " + thingName;
                }
                descriptor.setTitle(finalThingName);
                descriptor.setParams(ParamList{});
                qCDebug(dcSunSpec()) << "Auto appearing thing" << finalThingName;
                emit autoThingsAppeared({descriptor});
            }
        }
    }

    const auto isControllableFroniusStorage =
            modelsById.contains(SunSpecModelFactory::ModelIdNameplate) &&
            modelsById.contains(SunSpecModelFactory::ModelIdStorage) &&
            modelsById.contains(SunSpecModelFactory::ModelIdMppt);
    if (isControllableFroniusStorage) {
        auto controllableFroniusStorageAlreadyExists = false;
        foreach(Thing *childThing, myThings().filterByParentId(thing->id())) {
            if (childThing->thingClassId() == froniusControllableStorageThingClassId) {
                qCDebug(dcSunSpec()) << "Controllable storage already exists for this thing";
                initializeFroniusControllableStorageModels(childThing, connection);
                controllableFroniusStorageAlreadyExists = true;
                break;
            }
        }
        if (!controllableFroniusStorageAlreadyExists) {
            ThingDescriptor descriptor(froniusControllableStorageThingClassId);
            descriptor.setParentId(thing->id());
            const auto thingName = QT_TR_NOOP("Fronius Controllable Storage");
            descriptor.setTitle(thingName);
            descriptor.setParams(ParamList{});
            qCDebug(dcSunSpec()) << "Auto appearing thing" << thingName;
            emit autoThingsAppeared({descriptor});
        }
    }

    foreach (SunSpecModel *model, connection->models()) {
        Thing *modelThing = getThingForSunSpecModel(model->modelId(), model->modbusStartRegister(), thing->id());
        if (modelThing) {

            // We already set up a thing for this model. Make sure we are actually monitoring and updating it

            qCDebug(dcSunSpec()) << "Found" << modelThing << "for" << model;

            if (!isLimitableInverter &&
                    (modelThing->thingClassId() == sunspecSinglePhaseInverterThingClassId
                     || modelThing->thingClassId() == sunspecSplitPhaseInverterThingClassId
                     || modelThing->thingClassId() == sunspecThreePhaseInverterThingClassId)) {
                if (!m_sunSpecInverters.contains(modelThing)) {
                    m_sunSpecInverters.insert(modelThing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onInverterBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << modelThing;
                }
            } else if (modelThing->thingClassId() == sunspecSinglePhaseMeterThingClassId
                       || modelThing->thingClassId() == sunspecSplitPhaseMeterThingClassId
                       || modelThing->thingClassId() == sunspecThreePhaseMeterThingClassId) {
                if (!m_sunSpecMeters.contains(modelThing)) {
                    m_sunSpecMeters.insert(modelThing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onMeterBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << modelThing;
                }
            } else if (!isControllableFroniusStorage &&
                       modelThing->thingClassId() == sunspecStorageThingClassId) {
                if (!m_sunSpecStorages.contains(modelThing)) {
                    m_sunSpecStorages.insert(modelThing, model);
                    connect(model, &SunSpecModel::blockUpdated, this, &IntegrationPluginSunSpec::onStorageBlockUpdated);
                    qCDebug(dcSunSpec()) << "Model initialized successfully for" << modelThing;
                }
            }
        } else {

            // No thing for this model, let's see if we can create one

            switch (model->modelId()) {
                case SunSpecModelFactory::ModelIdCommon:
                    // Skip the common model, we already handled this one for each thing model
                    break;
                case SunSpecModelFactory::ModelIdInverterSinglePhase:
                case SunSpecModelFactory::ModelIdInverterSinglePhaseFloat:
                {
                    if (!isLimitableInverter) {
                        autocreateSunSpecModelThing(sunspecSinglePhaseInverterThingClassId, QT_TR_NOOP("Single Phase Inverter"), thing->id(), model);
                    }
                    break;
                }
                case SunSpecModelFactory::ModelIdInverterSplitPhase:
                case SunSpecModelFactory::ModelIdInverterSplitPhaseFloat:
                {
                    if (!isLimitableInverter) {
                        autocreateSunSpecModelThing(sunspecSplitPhaseInverterThingClassId, QT_TR_NOOP("Split Phase Inverter"), thing->id(), model);
                    }
                    break;
                }
                case SunSpecModelFactory::ModelIdInverterThreePhase:
                case SunSpecModelFactory::ModelIdInverterThreePhaseFloat:
                {
                    if (!isLimitableInverter) {
                        autocreateSunSpecModelThing(sunspecThreePhaseInverterThingClassId, QT_TR_NOOP("Three Phase Inverter"), thing->id(), model);
                    }
                    break;
                }
                case SunSpecModelFactory::ModelIdMeterSinglePhase:
                case SunSpecModelFactory::ModelIdMeterSinglePhaseFloat:
                    autocreateSunSpecModelThing(sunspecSinglePhaseMeterThingClassId, QT_TR_NOOP("Single Phase Meter"), thing->id(), model);
                    break;
                case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseAbn:
                case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseFloat:
                    autocreateSunSpecModelThing(sunspecSplitPhaseMeterThingClassId, QT_TR_NOOP("Split Phase Meter"), thing->id(), model);
                    break;
                case SunSpecModelFactory::ModelIdMeterThreePhase:
                case SunSpecModelFactory::ModelIdDeltaConnectThreePhaseAbcMeter:
                case SunSpecModelFactory::ModelIdMeterThreePhaseWyeConnect:
                case SunSpecModelFactory::ModelIdMeterThreePhaseDeltaConnect:
                    autocreateSunSpecModelThing(sunspecThreePhaseMeterThingClassId, QT_TR_NOOP("Three Phase Meter"), thing->id(), model);
                    break;
                case SunSpecModelFactory::ModelIdStorage:
                {
                    if (!isControllableFroniusStorage) {
                        autocreateSunSpecModelThing(sunspecStorageThingClassId, QT_TR_NOOP("Storage"), thing->id(), model);
                    }
                    break;
                }
                default:
                {
                    if (!isLimitableInverter && !isControllableFroniusStorage) {
                        qCWarning(dcSunSpec()) << "Plugin has no implementation for detected" << model;
                    }
                    break;
                }
            }
        }
    }

    // Check if we have a model for all child devices, otherwise we let the device dissappear
    // As of now there might be following situation where this code is required:
    //  - A setup has changed and something has been removed or replaced
    //  - Some SunSpec device seem to communicate different model id depending on the startup phase
    //    i.e. they communicate a SinglePhase Meter on register x, few minutes later it is a 3 phase meter on x
    // This code should handle such weird setups...

    if (connection->models().isEmpty())
        return;

    foreach (Thing *child, myThings().filterByParentId(thing->id())) {
        if (!m_modelIdParamTypeIds.contains(child->thingClassId()) || !m_modbusAddressParamTypeIds.contains(child->thingClassId()))
            continue;

        uint childModelId = child->paramValue(m_modelIdParamTypeIds.value(child->thingClassId())).toUInt();
        uint childModbusAddress = child->paramValue(m_modbusAddressParamTypeIds.value(child->thingClassId())).toUInt();

        bool modelFoundForChild = false;
        foreach (SunSpecModel *model, connection->models()) {
            if (childModelId == model->modelId() && childModbusAddress == model->modbusStartRegister()) {
                modelFoundForChild = true;
                break;
            }
        }

        if (!modelFoundForChild) {
            qCInfo(dcSunSpec()) << "The model for" << child << "does not seem to be available any more on" << connection << "Removing the device since it does not seem to exist ony more on this connection.";
            emit autoThingDisappeared(child->id());
        }
    }
}


SunSpecConnection *IntegrationPluginSunSpec::createConnection(Thing *thing)
{
    QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();
    int port = thing->paramValue(m_connectionPortParamTypeIds.value(thing->thingClassId())).toInt();
    int slaveId = thing->paramValue(m_connectionSlaveIdParamTypeIds.value(thing->thingClassId())).toInt();

    if (m_sunSpecConnections.contains(thing->id())) {
        qCDebug(dcSunSpec()) << "Reconfigure SunSpec connection" << thing;
        m_sunSpecConnections.take(thing->id())->deleteLater();
    }

    SunSpecConnection *connection = nullptr;
    if (thing->thingClassId() == solarEdgeConnectionThingClassId) {
        // Note: for some reason, solar edge is using big endian register order instead
        // of little endian as specified in sunspec and even solar edge documentation
        connection = new SunSpecConnection(address, port, slaveId, SunSpecDataPoint::ByteOrderBigEndian, this);
    } else {
        SunSpecDataPoint::ByteOrder endianness = SunSpecDataPoint::ByteOrderLittleEndian;
        QString endiannessParam = thing->paramValue("endianness").toString();
        if (endiannessParam == "Big Endian")
            endianness = SunSpecDataPoint::ByteOrderBigEndian;

        connection = new SunSpecConnection(address, port, slaveId, endianness, this);
    }
    connection->setTimeout(configValue(sunSpecPluginTimeoutParamTypeId).toUInt());
    connection->setNumberOfRetries(configValue(sunSpecPluginNumberOfRetriesParamTypeId).toUInt());

    // Reconnect on monitor reachable changed
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
        qCDebug(dcSunSpec()) << "Network device monitor reachable changed for" << thing->name() << reachable;
        if (!thing->setupComplete())
            return;

        if (reachable && !thing->stateValue("connected").toBool()) {
            qCDebug(dcSunSpec()) << "The monitor is reachable. Set the host address to" << monitor->networkDeviceInfo().address() << "and start connecting...";
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->connectDevice();
        }
    });

    m_sunSpecConnections.insert(thing->id(), connection);

    // Update all child things connected states for this connection
    connect(connection, &SunSpecConnection::connectedChanged, thing, [this, connection, thing] (bool connected) {
        if (connected) {
            qCDebug(dcSunSpec()) << connection << "connected";
        } else {
            qCWarning(dcSunSpec()) << connection << "disconnected";
        }

        thing->setStateValue("connected", connected);

        if (connected) {
            if (thing->setupComplete()) {
                // We rediscovery all models on every connect to keep the child things in sync
                connection->startDiscovery();
            }
        }
        // Update connected state of child things
        foreach (Thing *child, myThings().filterByParentId(thing->id())) {
            if (connected) {
                child->setStateValue("connected", true);
            } else {
                markThingStatesDisconnected(child);
            }
        }
    });

    connect(connection, &SunSpecConnection::discoveryFinished, thing, [this, connection, thing] (bool success) {
        if (success) {
            qCDebug(dcSunSpec()) << "Discovery finished successfully of" << connection;
            processDiscoveryResult(thing, connection);
        } else {
            qCWarning(dcSunSpec()) << "Discovery finished with errors on" << connection;
        }
    });

    return connection;
}

void IntegrationPluginSunSpec::setupSolarEdgeBattery(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    int modbusStartRegister = thing->paramValue(solarEdgeBatteryThingModbusAddressParamTypeId).toUInt();
    SunSpecConnection *connection = m_sunSpecConnections.value(thing->parentId());
    if (!connection) {
        qCWarning(dcSunSpec()) << "Could not find SunSpec parent connection for sunspec battery" << thing;
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    qCDebug(dcSunSpec()) << "Setting up SolarEdge battery...";
    SolarEdgeBattery *battery = new SolarEdgeBattery(thing, connection, modbusStartRegister, connection);

    m_sunSpecThings.insert(thing, battery);
    connect(battery, &SolarEdgeBattery::blockDataUpdated, this, &IntegrationPluginSunSpec::onSolarEdgeBatteryBlockUpdated);
    info->finish(Thing::ThingErrorNoError);

    // Start initializing battery data
    if (connection->connected())
        battery->init();
}

void IntegrationPluginSunSpec::searchSolarEdgeBatteries(SunSpecConnection *connection)
{
    if (!connection->connected()) {
        qCDebug(dcSunSpec()) << "Could not search for SolarEdge batteries, the connection does not seem to be available at the moment.";
        return;
    }

    qCDebug(dcSunSpec()) << "Searching for connected SolarEdge batteries...";
    ThingId parentThingId = m_sunSpecConnections.key(connection);
    if (parentThingId.isNull()) {
        qCWarning(dcSunSpec()) << "Could not search for SolarEdge batteries because of find parent ThingId connection for" << connection->hostAddress().toString();
        return;
    }

    // Batteries are not mapped to the sunspec layer, so we have to treat them as normal modbus registers.
    // Read the battery id to verify if the battery is connected.
    // Battery 1: start register 0xE100, device id register 0xE140
    // Battery 2: start register 0xE200, device id register 0xE240
    searchSolarEdgeBattery(connection, parentThingId, 0xE100);
    searchSolarEdgeBattery(connection, parentThingId, 0xE200);
}

void IntegrationPluginSunSpec::searchSolarEdgeBattery(SunSpecConnection *connection, const ThingId &parentThingId, quint16 startRegister)
{
    // Read the battery data, if init failed there is no battery connected,
    // otherwise there is a battery and we should check if we need to setup the battery thing

    // Create a temporary battery object without thing for init
    qCDebug(dcSunSpec()) << "Checking presence of SolarEdge battery on modbus register" << startRegister;
    SolarEdgeBattery *battery = new SolarEdgeBattery(nullptr, connection, startRegister, connection);
    connect(battery, &SolarEdgeBattery::initFinished, connection, [=](bool success) {
        // Delete this object since we used it only for set up
        battery->deleteLater();

        // If init failed, no battery connected
        if (!success) {
            qCDebug(dcSunSpec()) << "No SolarEdge battery connected on register" << startRegister << "- not creating thing.";
            return;
        }

        qCDebug(dcSunSpec()) << "Battery initialized successfully." << battery->batteryData().manufacturerName << battery->batteryData().model;
        // Check if we already created this battery
        if (!myThings().filterByParam(solarEdgeBatteryThingSerialNumberParamTypeId, battery->batteryData().serialNumber).isEmpty()) {
            qCDebug(dcSunSpec()) << "Battery already set up" << battery->batteryData().serialNumber;
        } else {
            // Create new battery device in the system
            ThingDescriptor descriptor(solarEdgeBatteryThingClassId, battery->batteryData().manufacturerName + " - " + battery->batteryData().model, QString(), parentThingId);
            ParamList params;
            params.append(Param(solarEdgeBatteryThingModbusAddressParamTypeId, startRegister));
            params.append(Param(solarEdgeBatteryThingManufacturerParamTypeId, battery->batteryData().manufacturerName));
            params.append(Param(solarEdgeBatteryThingDeviceModelParamTypeId, battery->batteryData().model));
            params.append(Param(solarEdgeBatteryThingSerialNumberParamTypeId, battery->batteryData().serialNumber));
            descriptor.setParams(params);
            emit autoThingsAppeared({descriptor});
        }
    });

    // Try to initialize battery data
    battery->init();
}

double IntegrationPluginSunSpec::calculateSolarEdgePvProduction(Thing *thing, double acPower, double dcPower)
{
    // Note: for solar edge, we need to calculate pv power from battery and AC/DC current depending
    // on loading or discharging of the battery

    double pvPower = acPower;
    Thing *parentThing = myThings().findById(thing->parentId());
    if (parentThing && parentThing->thingClassId() == solarEdgeConnectionThingClassId) {
        SolarEdgeBattery *battery = nullptr;
        // This is a solar edge, let's see if we have a battery for this connection
        foreach (Thing *sunspecThing, m_sunSpecThings.keys()) {
            if (sunspecThing->thingClassId() == solarEdgeBatteryThingClassId && thing->parentId() == parentThing->id()) {
                battery = qobject_cast<SolarEdgeBattery *>(m_sunSpecThings.value(sunspecThing));
                break;
            }
        }

        // Check if we have a meter...
        Thing *meterThing = nullptr;
        foreach (Thing *sunspecThing, m_sunSpecMeters.keys()) {
            if (thing->parentId() == parentThing->id()) {
                meterThing = sunspecThing;
                break;
            }
        }

        // This is a solar edge, let's see if we have a battery for this connection
        if (battery) {
            double meterCurrentPower = meterThing ? meterThing->stateValue("currentPower").toDouble() : 0;
            qCDebug(dcSunSpec()) << "SolarEdge: found battery for inverter: calculate actual PV power from battery DC power and inverter DC power...";
            qCDebug(dcSunSpec()) << "--> SolarEdge: -------------------------------------------------------";
            qCDebug(dcSunSpec()) << "--> SolarEdge: meter power:" << meterCurrentPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: inverter AC power:" << acPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: inverter DC power:" << dcPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: battery DC power:" << battery->batteryData().instantaneousPower << "W";
            qCDebug(dcSunSpec()) << "--> SolarEdge: battery DC state:" << battery->batteryData().batteryStatus;

            switch (battery->batteryData().batteryStatus) {
            case SolarEdgeBattery::BatteryStatus::Charge: {
                // Actual PV = inverter AC power - battery power
                pvPower = acPower - battery->batteryData().instantaneousPower;
                qCDebug(dcSunSpec()) << "--> SolarEdge: calculate actual PV power: inverter AC power - battery power:" << acPower << "-" << battery->batteryData().instantaneousPower << "=" << pvPower;
                break;
            }
            case SolarEdgeBattery::BatteryStatus::Discharge: {
                // Actual PV = inverter DC power + battery power
                pvPower = dcPower - battery->batteryData().instantaneousPower;
                qCDebug(dcSunSpec()) << "--> SolarEdge: calculate actual PV power: inverter DC power - battery power:" << dcPower << "-" << battery->batteryData().instantaneousPower << "=" << pvPower;
                if (pvPower > 0) {
                    qCDebug(dcSunSpec()) << "--> SolarEdge: actual PV power: 0W | loss:" << pvPower << "W";
                    pvPower = 0;
                }
                break;
            }
            default:
                // Idle: No change required, AC power is actual PV energy
                pvPower = acPower;
                break;
            }

            // Calculate self consumption
            double selfConsumption = pvPower - meterCurrentPower + battery->batteryData().instantaneousPower;
            qCDebug(dcSunSpec()) << "--> SolarEdge: self consumption" << selfConsumption << "W";
        }
    }

    return pvPower;
}

void IntegrationPluginSunSpec::autocreateSunSpecModelThing(const ThingClassId &thingClassId, const QString &thingName, const ThingId &parentId, SunSpecModel *model)
{
    /* On some systems we get duplicated things having the same serial number but are on different registers.
     * Those devices normally dissapear after some time again. For now, we need to prevent to even create those
     * devices by filtering out those who already have been added using the serialnumber, not only the combination
     * of model number and start address. */

    QString serialNumber = model->commonModelInfo().serialNumber;
    if (!serialNumber.isEmpty()) {
        // If we have any model with this exact serial number, we skip adding this model
        foreach (Thing *thing, myThings()) {
            if (m_serialNumberParamTypeIds.contains(thing->thingClassId())) {
                QString thingSerialNumber = thing->paramValue(m_serialNumberParamTypeIds.value(thing->thingClassId())).toString();
                if (serialNumber == thingSerialNumber) {
                    qCWarning(dcSunSpec()) << "Trying to add a new thing for a SunSpec model, but we already have a thing with this serial number. Not adding it to the system...";
                    qCWarning(dcSunSpec()) << "--> Already added:" << thing << thing->params();
                    qCWarning(dcSunSpec()) << "--> Attempt to add:" << thingName << model
                                           << model->commonModelInfo().modelName
                                           << model->commonModelInfo().manufacturerName
                                           << model->commonModelInfo().serialNumber
                                           << model->commonModelInfo().versionString;

                    return;
                }
            }
        }
    }

    ThingDescriptor descriptor(thingClassId);
    descriptor.setParentId(parentId);
    QString finalThingName;
    if (model->commonModelInfo().manufacturerName.isEmpty()) {
        finalThingName = thingName;
    } else {
        finalThingName = model->commonModelInfo().manufacturerName + " " + thingName;
    }

    descriptor.setTitle(finalThingName);

    ParamList params;
    params.append(Param(m_modelIdParamTypeIds.value(descriptor.thingClassId()), model->modelId()));
    params.append(Param(m_modbusAddressParamTypeIds.value(descriptor.thingClassId()), model->modbusStartRegister()));
    params.append(Param(m_manufacturerParamTypeIds.value(descriptor.thingClassId()), model->commonModelInfo().manufacturerName));
    params.append(Param(m_deviceModelParamTypeIds.value(descriptor.thingClassId()), model->commonModelInfo().modelName));
    params.append(Param(m_serialNumberParamTypeIds.value(descriptor.thingClassId()), model->commonModelInfo().serialNumber));
    descriptor.setParams(params);
    emit autoThingsAppeared({descriptor});
}

QString IntegrationPluginSunSpec::getInverterStateString(quint16 state)
{
    // Note: this works because the models have all the same value
    QString stateString;
    switch (state) {
    case SunSpecInverterSinglePhaseModel::StOff:
        stateString = "Off";
        break;
    case SunSpecInverterSinglePhaseModel::StSleeping:
        stateString = "Sleeping";
        break;
    case SunSpecInverterSinglePhaseModel::StStarting:
        stateString = "Starting";
        break;
    case SunSpecInverterSinglePhaseModel::StMppt:
        stateString = "MPPT";
        break;
    case SunSpecInverterSinglePhaseModel::StThrottled:
        stateString = "Throttled";
        break;
    case SunSpecInverterSinglePhaseModel::StShuttingDown:
        stateString = "Shutting down";
        break;
    case SunSpecInverterSinglePhaseModel::StFault:
        stateString = "Fault";
        break;
    case SunSpecInverterSinglePhaseModel::StStandby:
        stateString = "Standby";
        break;
    }
    return stateString;
}

QString IntegrationPluginSunSpec::getInverterErrorString(quint32 flag)
{
    // Note: this works because the models have all the same value
    QStringList errorStrings;
    SunSpecInverterSinglePhaseModel::Evt1Flags event1Flag = static_cast<SunSpecInverterSinglePhaseModel::Evt1Flags>(flag);
    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1GroundFault)) {
        errorStrings.append(QT_TR_NOOP("Ground fault"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1DcOverVolt)) {
        errorStrings.append(QT_TR_NOOP("DC over voltage"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1AcDisconnect)) {
        errorStrings.append(QT_TR_NOOP("AC disconnect"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1DcDisconnect)) {
        errorStrings.append(QT_TR_NOOP("DC disconnect"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1GridDisconnect)) {
        errorStrings.append(QT_TR_NOOP("Grid disconnect"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1CabinetOpen)) {
        errorStrings.append(QT_TR_NOOP("Cabinet open"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1ManualShutdown)) {
        errorStrings.append(QT_TR_NOOP("Manual shutdown"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1OverTemp)) {
        errorStrings.append(QT_TR_NOOP("Over temperature"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1OverFrequency)) {
        errorStrings.append(QT_TR_NOOP("Over frequency"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1UnderFrequency)) {
        errorStrings.append(QT_TR_NOOP("Under frequency"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1AcOverVolt)) {
        errorStrings.append(QT_TR_NOOP("AC over voltage"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1AcUnderVolt)) {
        errorStrings.append(QT_TR_NOOP("AC under voltage"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1BlownStringFuse)) {
        errorStrings.append(QT_TR_NOOP("Blown string fuse"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1UnderTemp)) {
        errorStrings.append(QT_TR_NOOP("Under temperature"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1MemoryLoss)) {
        errorStrings.append(QT_TR_NOOP("Memory loss"));
    }

    if (event1Flag.testFlag(SunSpecInverterSinglePhaseModel::Evt1HwTestFailure)) {
        errorStrings.append(QT_TR_NOOP("Hardware test failure"));
    }

    if (errorStrings.isEmpty()) {
        return QT_TR_NOOP("No error");
    } else {
        return errorStrings.join(", ");
    }
}

double IntegrationPluginSunSpec::fixValueSign(double targetValue, double powerValue)
{
    // Some sunspec devices (i.e. SolarEdge return an absolute value on the phase current.
    // This method makes sure the phase current has the same sign as the phase power value.
    bool sameSign = ((targetValue < 0) == (powerValue < 0));
    return (sameSign ? targetValue : -targetValue);
}

bool IntegrationPluginSunSpec::hasManufacturer(const QStringList &manufacturers, const QString &manufacturer)
{
    foreach (const QString &name, manufacturers) {
        if (name.toLower().contains(manufacturer.toLower())) {
            return true;
        }
    }

    return false;
}

void IntegrationPluginSunSpec::markThingStatesDisconnected(Thing *thing)
{
    qCDebug(dcSunSpec()) << thing << "is now disconnected. Setting energy live data to 0.";
    if (thing->thingClassId() == sunspecSinglePhaseInverterThingClassId) {
        thing->setStateValue(sunspecSinglePhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterPhaseVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecSplitPhaseInverterThingClassId) {
        thing->setStateValue(sunspecSplitPhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseANVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseACurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterPhaseBCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecThreePhaseInverterThingClassId) {
        thing->setStateValue(sunspecThreePhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseANVoltageStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseBNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseCNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseACurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseBCurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterPhaseCCurrentStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId) {
        thing->setStateValue(sunspecLimitableSinglePhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterPhaseVoltageStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId) {
        thing->setStateValue(sunspecLimitableSplitPhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseANVoltageStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseBNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseACurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseBCurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
        thing->setStateValue(sunspecLimitableThreePhaseInverterConnectedStateTypeId, false);
        thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterFrequencyStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseANVoltageStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseBNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseCNVoltageStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseACurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseBCurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseCCurrentStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterVoltageDcStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentDcStateTypeId, 0);
        thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentPowerDcStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecSinglePhaseMeterThingClassId) {
        thing->setStateValue(sunspecSinglePhaseMeterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseMeterVoltagePhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSinglePhaseMeterFrequencyStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecSplitPhaseMeterThingClassId) {
        thing->setStateValue(sunspecSplitPhaseMeterConnectedStateTypeId, false);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterTotalCurrentStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterLnACVoltageStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseAStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseBStateTypeId, 0);
        thing->setStateValue(sunspecSplitPhaseMeterFrequencyStateTypeId,0 );
    } else if (thing->thingClassId() == sunspecThreePhaseMeterThingClassId) {
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, false);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, 0);
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, 0);
    } else if (thing->thingClassId() == sunspecStorageThingClassId) {
        thing->setStateValue(sunspecStorageConnectedStateTypeId, false);
        thing->setStateValue(sunspecStorageGridChargingStateTypeId, false);
        thing->setStateValue(sunspecStorageChargingRateStateTypeId, 0);
        thing->setStateValue(sunspecStorageDischargingRateStateTypeId, 0);
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, "Off");
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, "idle");
    } else if (thing->thingClassId() == froniusControllableStorageThingClassId) {
        thing->setStateValue(froniusControllableStorageConnectedStateTypeId, false);
        thing->setStateValue(froniusControllableStorageBatteryCriticalStateTypeId, false);
        thing->setStateValue(froniusControllableStorageBatteryLevelStateTypeId, 0);
        thing->setStateValue(froniusControllableStorageChargingStateStateTypeId, "idle");
        thing->setStateValue(froniusControllableStorageStorageStatusStateTypeId, "Off");
        thing->setStateValue(froniusControllableStorageCurrentPowerStateTypeId, 0);
    } else if (thing->thingClassId() == solarEdgeBatteryThingClassId) {
        thing->setStateValue(solarEdgeBatteryConnectedStateTypeId, false);
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Idle");
        thing->setStateValue(solarEdgeBatteryChargingStateStateTypeId, "idle");
        thing->setStateValue(solarEdgeBatteryInstantaneousVoltageStateTypeId, 0);
        thing->setStateValue(solarEdgeBatteryInstantaneousCurrentStateTypeId, 0);
        thing->setStateValue(solarEdgeBatteryCurrentPowerStateTypeId, 0);
    }

}

void IntegrationPluginSunSpec::initializeLimitableInverterModels(Thing *thing,
                                                                 SunSpecConnection *connection)
{
    foreach (SunSpecModel *model, connection->models()) {
        if (model->modelId() == SunSpecModelFactory::ModelIdInverterSinglePhase ||
                model->modelId() == SunSpecModelFactory::ModelIdInverterSinglePhaseFloat ||
                model->modelId() == SunSpecModelFactory::ModelIdInverterSplitPhase ||
                model->modelId() == SunSpecModelFactory::ModelIdInverterSplitPhaseFloat ||
                model->modelId() == SunSpecModelFactory::ModelIdInverterThreePhase ||
                model->modelId() == SunSpecModelFactory::ModelIdInverterThreePhaseFloat) {
            if (!m_sunSpecInverters.contains(thing)) {
                connect(model, &SunSpecModel::blockUpdated,
                        this, &IntegrationPluginSunSpec::onInverterBlockUpdated);
                m_sunSpecInverters.insert(thing, model);
                qCDebug(dcSunSpec()) << "Inverter model successfully initialized for" << thing->name();
            }
        } else if (model->modelId() == SunSpecModelFactory::ModelIdSettings) {
            if (!m_sunSpecSettings.contains(thing)) {
                connect(model, &SunSpecModel::blockUpdated,
                        this, &IntegrationPluginSunSpec::onSettingsBlockUpdated);
                m_sunSpecSettings.insert(thing, model);
                qCDebug(dcSunSpec()) << "Settings model successfully initialized for" << thing->name();
            }
        } else if (model->modelId() == SunSpecModelFactory::ModelIdControls) {
            if (!m_sunSpecControls.contains(thing)) {
                connect(model, &SunSpecModel::blockUpdated,
                        this, &IntegrationPluginSunSpec::onControlsBlockUpdated);
                m_sunSpecControls.insert(thing, model);
                const auto controlsModel = qobject_cast<SunSpecControlsModel *>(model);
                setupControlsModel(controlsModel);
                qCDebug(dcSunSpec()) << "Controls model successfully initialized for" << thing->name();
            }
        }
    }
}

void IntegrationPluginSunSpec::setEnableExportLimit(Thing *thing,
                                                    bool enableLimit,
                                                    ThingActionInfo *info)
{
    const auto valueToSet = enableLimit ?
                SunSpecControlsModel::Wmaxlim_enaEnabled :
                SunSpecControlsModel::Wmaxlim_enaDisabled;
    qCDebug(dcSunSpec()) << "Setting WMaxLim_Ena to" << valueToSet << "for thing" << thing->name();
    SunSpecControlsModel *controls = qobject_cast<SunSpecControlsModel *>(m_sunSpecControls.value(thing));
    if (!controls) {
        qWarning(dcSunSpec()) << "Could not find sunspec controls model for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
        return;
    }
    const auto reply = controls->setWMaxLimEna(valueToSet);
    if (!reply) {
        if (info) {
            info->finish(Thing::ThingErrorHardwareFailure);
        }
        qWarning(dcSunSpec()) << "Unable to set WMaxLim_Ena for thing" << thing->name();
        return;
    }
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, reply, [info, reply, thing]{
        if (reply->error() != QModbusDevice::NoError) {
            qWarning(dcSunSpec()) << "Unable to set WMaxLim_Ena for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
            return;
        }
        if (info) {
            info->finish(Thing::ThingErrorNoError);
        }
    });
}

QModbusReply *IntegrationPluginSunSpec::setExportLimit(Thing *thing,
                                                       float exportLimit,
                                                       ThingActionInfo *info)
{
    qCDebug(dcSunSpec()) << "Setting WMaxLimPct to" << exportLimit << "% for thing" << thing->name();
    SunSpecControlsModel *controls = qobject_cast<SunSpecControlsModel *>(m_sunSpecControls.value(thing));
    if (!controls) {
        qWarning(dcSunSpec()) << "Could not find sunspec controls model for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
        return nullptr;
    }
    const auto reply = controls->setWMaxLimPct(exportLimit);
    if (!reply) {
        if (info) {
            info->finish(Thing::ThingErrorHardwareFailure);
        }
        qWarning(dcSunSpec()) << "Unable to set WMaxLimPct for thing" << thing->name();
        return nullptr;
    }
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, reply, [info, reply, thing, exportLimit]{
        if (reply->error() != QModbusDevice::NoError) {
            qWarning(dcSunSpec()) << "Unable to set WMaxLimPct for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
            return;
        }
        const auto exportLimitWatt = exportLimit * thing->stateValue("maxPowerOutput").toFloat() / 100.f;
        thing->setStateValue("exportLimit", exportLimitWatt);
        if (info) {
            info->finish(Thing::ThingErrorNoError);
        }
    });
    return reply;
}

void IntegrationPluginSunSpec::setupControlsModel(SunSpecControlsModel *model)
{
    if (!model) {
        qCWarning(dcSunSpec()) << "Can not set up invalid model";
        return;
    }

    // Set WMaxLimPct_WinTms, WMaxLimPct_RvrtTms und WMaxLimPct_RmpTms to 0 to
    // apply changes to the WMaxLimPct settings immediately and without timeout.
    const auto wMaxLimPct_WinTmsReply = model->setWMaxLimPctWinTms(0);
    connect(wMaxLimPct_WinTmsReply, &QModbusReply::finished,
            wMaxLimPct_WinTmsReply, &QModbusReply::deleteLater);
    connect(wMaxLimPct_WinTmsReply, &QModbusReply::finished,
            wMaxLimPct_WinTmsReply, [wMaxLimPct_WinTmsReply, model] () {
        if (wMaxLimPct_WinTmsReply->error() != QModbusDevice::NoError) {
            qCWarning(dcSunSpec()) << "Unable to set WMaxLimPct_WinTms";
            return;
        }

        const auto wMaxLimPct_RvrtTmsReply = model->setWMaxLimPctRvrtTms(0);
        connect(wMaxLimPct_RvrtTmsReply, &QModbusReply::finished,
                wMaxLimPct_RvrtTmsReply, &QModbusReply::deleteLater);
        connect(wMaxLimPct_RvrtTmsReply, &QModbusReply::finished,
                wMaxLimPct_RvrtTmsReply, [wMaxLimPct_RvrtTmsReply, model] () {
            if (wMaxLimPct_RvrtTmsReply->error() != QModbusDevice::NoError) {
                qCWarning(dcSunSpec()) << "Unable to set WMaxLimPct_RvrtTms";
                return;
            }

            const auto wMaxLimPct_RmpTmsReply = model->setWMaxLimPctRmpTms(0);
            connect(wMaxLimPct_RmpTmsReply, &QModbusReply::finished,
                    wMaxLimPct_RmpTmsReply, &QModbusReply::deleteLater);
            connect(wMaxLimPct_RmpTmsReply, &QModbusReply::finished,
                    wMaxLimPct_RmpTmsReply, [wMaxLimPct_RmpTmsReply] () {
                if (wMaxLimPct_RmpTmsReply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSunSpec()) << "Unable to set WMaxLimPct_RmpTms";
                    return;
                }
            });
        });
    });
}

void IntegrationPluginSunSpec::initializeFroniusControllableStorageModels(Thing *thing, SunSpecConnection *connection)
{
    foreach (SunSpecModel *model, connection->models()) {
        if (model->modelId() == SunSpecModelFactory::ModelIdNameplate) {
            if (!m_sunSpecNameplates.contains(thing)) {
                connect(model, &SunSpecModel::blockUpdated,
                        this, &IntegrationPluginSunSpec::onNameplateBlockUpdated);
                m_sunSpecNameplates.insert(thing, model);
                qCDebug(dcSunSpec()) << "Nameplate model successfully initialized for" << thing->name();
            }
        } else if (model->modelId() == SunSpecModelFactory::ModelIdStorage) {
            if (!m_sunSpecStorages.contains(thing)) {
                connect(model, &SunSpecModel::blockUpdated,
                        this, &IntegrationPluginSunSpec::onStorageBlockUpdated);
                m_sunSpecStorages.insert(thing, model);
                const auto storageModel = qobject_cast<SunSpecStorageModel *>(model);
                setupStorageModel(storageModel);
                qCDebug(dcSunSpec()) << "Storage control model successfully initialized for" << thing->name();
            }
        } else if (model->modelId() == SunSpecModelFactory::ModelIdMppt) {
            if (!m_sunSpecMppts.contains(thing)) {
                connect(model, &SunSpecModel::blockUpdated,
                        this, &IntegrationPluginSunSpec::onMpptBlockUpdated);
                m_sunSpecMppts.insert(thing, model);
                qCDebug(dcSunSpec()) << "MPPT model successfully initialized for" << thing->name();
            }
        }
    }
}

QModbusReply *IntegrationPluginSunSpec::setEnableForcePower(Thing *thing,
                                                            bool enableForcePower,
                                                            ThingActionInfo *info)
{
    qCDebug(dcSunSpec()) << "Setting enable force power to" << enableForcePower << "for thing" << thing->name();
    SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
    if (!storage) {
        qWarning(dcSunSpec()) << "Could not find sunspec storage model for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
        return nullptr;
    }

    auto storCtl_ModToSet = SunSpecStorageModel::Storctl_modFlags{};
    storCtl_ModToSet.setFlag(SunSpecStorageModel::Storctl_modCharge, enableForcePower);
    storCtl_ModToSet.setFlag(SunSpecStorageModel::Storctl_modDiScharge, enableForcePower);

    qCDebug(dcSunSpec()) << "Setting StorCtl_Mod to" << storCtl_ModToSet;
    const auto reply = storage->setStorCtlMod(storCtl_ModToSet);
    if (!reply) {
        if (info) {
            info->finish(Thing::ThingErrorHardwareFailure);
        }
        qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
        return nullptr;
    }

    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, reply, [info, reply, thing]{
        if (reply->error() != QModbusDevice::NoError) {
            qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
            return;
        }
        if (info) {
            info->finish(Thing::ThingErrorNoError);
        }
    });
    return reply;
}

void IntegrationPluginSunSpec::setForcePower(Thing *thing,
                                             double forcePower,
                                             ThingActionInfo *info)
{
    qCDebug(dcSunSpec()) << "Setting force power to" << forcePower << "for thing" << thing->name();
    SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
    if (!storage) {
        qWarning(dcSunSpec()) << "Could not find sunspec storage model for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
        return;
    }

    const auto wChaMax = storage->wChaMax();
    if (qFuzzyIsNull(wChaMax)) {
        qWarning(dcSunSpec()) << "WChaMax is 0 for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
        return;
    }
    const auto forcePowerPercentage = 100 * forcePower / wChaMax;
    qCDebug(dcSunSpec()) << forcePower << "/" << wChaMax << "=" << forcePowerPercentage << "%";

    auto inWRteToSet = forcePowerPercentage;
    auto outWRteToSet = -forcePowerPercentage;

    qCDebug(dcSunSpec()) << "Setting InWRte (charge limit) to" << inWRteToSet;
    const auto inWRteReply = storage->setInWRte(inWRteToSet);
    if (!inWRteReply) {
        qWarning(dcSunSpec()) << "Unable to set InWRte for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareFailure);
        }
        return;
    }
    connect(inWRteReply, &QModbusReply::finished, inWRteReply, &QModbusReply::deleteLater);
    connect(inWRteReply, &QModbusReply::finished, inWRteReply,
            [info, inWRteReply, thing, outWRteToSet, storage]() {
        if (inWRteReply->error() != QModbusDevice::NoError) {
            qWarning(dcSunSpec()) << "Unable to set InWRte for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
            return;
        }

        qCDebug(dcSunSpec()) << "Setting OutWRte (discharge limit) to" << outWRteToSet;
        const auto outWRteReply = storage->setOutWRte(outWRteToSet);
        if (!outWRteReply) {
            qWarning(dcSunSpec()) << "Unable to set OutWRte for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
        }
        connect(outWRteReply, &QModbusReply::finished, outWRteReply, &QModbusReply::deleteLater);
        connect(outWRteReply, &QModbusReply::finished, outWRteReply, [info, outWRteReply, thing] {
            if (outWRteReply->error() != QModbusDevice::NoError) {
                qWarning(dcSunSpec()) << "Unable to set OutWRte for thing" << thing->name();
                if (info) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
                return;
            }

            if (info) {
                info->finish(Thing::ThingErrorNoError);
            }
        });
    });
}

void IntegrationPluginSunSpec::setChargingAllowed(Thing *thing,
                                                  bool chargingAllowed,
                                                  ThingActionInfo *info)
{
    SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(m_sunSpecStorages.value(thing));
    if (!storage) {
        qWarning(dcSunSpec()) << "setChargingAllowed: Could not find sunspec storage model for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
        return;
    }

    auto storCtl_Mod = storage->storCtlMod();
    const auto currentInWRte = storage->inWRte();
    if (chargingAllowed) {
        if (!storCtl_Mod.testFlag(SunSpecStorageModel::Storctl_modCharge) ||
                (storCtl_Mod.testFlag(SunSpecStorageModel::Storctl_modCharge) &&
                 qFuzzyCompare(currentInWRte, 100.f))) {
            if (info) {
                info->finish(Thing::ThingErrorNoError);
            }
            return;
        }
    } else {
        if (storCtl_Mod.testFlag(SunSpecStorageModel::Storctl_modCharge) &&
                qFuzzyIsNull(currentInWRte)) {
            if (info) {
                info->finish(Thing::ThingErrorNoError);
            }
            return;
        }
    }

    qCDebug(dcSunSpec()) << "Setting charging allowed to" << chargingAllowed << "for thing" << thing->name();
    qCDebug(dcSunSpec()) << "Setting StorCtl_Mod to" << storCtl_Mod;
    storCtl_Mod.setFlag(SunSpecStorageModel::Storctl_modCharge, !chargingAllowed);
    const auto storCtl_ModReply = storage->setStorCtlMod(storCtl_Mod);
    if (!storCtl_ModReply) {
        qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
        if (info) {
            info->finish(Thing::ThingErrorHardwareFailure);
        }
        return;
    }

    connect(storCtl_ModReply, &QModbusReply::finished, storCtl_ModReply, &QModbusReply::deleteLater);
    connect(storCtl_ModReply, &QModbusReply::finished, storCtl_ModReply,
            [info, storCtl_ModReply, thing, chargingAllowed, storage]() {
        if (storCtl_ModReply->error() != QModbusDevice::NoError) {
            qWarning(dcSunSpec()) << "Unable to set StorCtl_Mod for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
            return;
        }

        const auto inWRteToSet = chargingAllowed ? 100.f : 0.f;
        qCDebug(dcSunSpec()) << "Setting InWRte to" << inWRteToSet;
        const auto inWRteReply = storage->setInWRte(inWRteToSet);
        if (!inWRteReply) {
            qWarning(dcSunSpec()) << "Unable to set InWRte for thing" << thing->name();
            if (info) {
                info->finish(Thing::ThingErrorHardwareFailure);
            }
            return;
        }
        connect(inWRteReply, &QModbusReply::finished, inWRteReply, &QModbusReply::deleteLater);
        connect(inWRteReply, &QModbusReply::finished, inWRteReply, [info, inWRteReply, thing]{
            if (inWRteReply->error() != QModbusDevice::NoError) {
                qWarning(dcSunSpec()) << "Unable to set InWRte for thing" << thing->name();
                if (info) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
                return;
            }

            if (info) {
                info->finish(Thing::ThingErrorNoError);
            }
        });
    });
}

void IntegrationPluginSunSpec::setupStorageModel(SunSpecStorageModel *model)
{
    if (!model) {
        qCWarning(dcSunSpec()) << "Can not set up invalid model";
        return;
    }

    // Set InOutWRte_RvrtTms to 0 to apply changes to InWRte and OutWRte without timeout
    // and ChaGriSet to true to allow charging the battery from the grid.
    const auto inOutWRte_RvrtTmsReply = model->setInOutWRteRvrtTms(0);
    connect(inOutWRte_RvrtTmsReply, &QModbusReply::finished,
            inOutWRte_RvrtTmsReply, &QModbusReply::deleteLater);
    connect(inOutWRte_RvrtTmsReply, &QModbusReply::finished,
            inOutWRte_RvrtTmsReply, [inOutWRte_RvrtTmsReply, model] () {
        if (inOutWRte_RvrtTmsReply->error() != QModbusDevice::NoError) {
            qCWarning(dcSunSpec()) << "Unable to set InOutWRte_RvrtTms";
            return;
        }

        const auto chaGriSetReply = model->setChaGriSet(SunSpecStorageModel::ChagrisetGrid);
        connect(chaGriSetReply, &QModbusReply::finished,
                chaGriSetReply, &QModbusReply::deleteLater);
        connect(chaGriSetReply, &QModbusReply::finished,
                chaGriSetReply, [chaGriSetReply] () {
            if (chaGriSetReply->error() != QModbusDevice::NoError) {
                qCWarning(dcSunSpec()) << "Unable to set WMaxLimPct_RmpTms";
                return;
            }
        });
    });
}

void IntegrationPluginSunSpec::onRefreshTimer()
{
    // Update meters
    foreach (SunSpecModel *model, m_sunSpecMeters.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update storage
    foreach (SunSpecModel *model, m_sunSpecStorages.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update all other sunspec thing blocks
    foreach (SunSpecThing *sunSpecThing, m_sunSpecThings) {
        if (sunSpecThing->connection()->connected()) {
            sunSpecThing->readBlockData();
        }
    }

    // Update inverters
    foreach (SunSpecModel *model, m_sunSpecInverters.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update settings models
    foreach (SunSpecModel *model, m_sunSpecSettings.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update controls models
    foreach (SunSpecModel *model, m_sunSpecControls.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update nameplate models
    foreach (SunSpecModel *model, m_sunSpecNameplates.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }

    // Update MPPT models
    foreach (SunSpecModel *model, m_sunSpecMppts.values()) {
        if (model->connection()->connected()) {
            model->readBlockData();
        }
    }
}

void IntegrationPluginSunSpec::onPluginConfigurationChanged(const ParamTypeId &paramTypeId, const QVariant &value)
{
    // Check refresh schedule
    if (paramTypeId == sunSpecPluginUpdateIntervalParamTypeId) {
        qCDebug(dcSunSpec()) << "Update interval has changed" << value.toInt();
        if (m_refreshTimer) {
            int refreshTime = value.toInt();
            m_refreshTimer->stop();
            hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
            m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(refreshTime);
            connect(m_refreshTimer, &PluginTimer::timeout, this, &IntegrationPluginSunSpec::onRefreshTimer);
            m_refreshTimer->start();
        }
    } else if (paramTypeId == sunSpecPluginNumberOfRetriesParamTypeId) {
        qCDebug(dcSunSpec()) << "Updating number of retries" << value.toUInt();
        foreach (SunSpecConnection *connection, m_sunSpecConnections) {
            connection->setNumberOfRetries(value.toUInt());
        }
    } else if (paramTypeId == sunSpecPluginTimeoutParamTypeId) {
        qCDebug(dcSunSpec()) << "Updating timeout" << value.toUInt() << "[ms]";
        foreach (SunSpecConnection *connection, m_sunSpecConnections) {
            connection->setTimeout(value.toUInt());
        }
    } else {
        qCWarning(dcSunSpec()) << "Unknown plugin configuration" << paramTypeId << "Value" << value;
    }
}

void IntegrationPluginSunSpec::onInverterBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecInverters.key(model);
    if (!thing) return;

    // Get parent thing
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing)
        return;

    switch (model->modelId()) {
    case SunSpecModelFactory::ModelIdInverterSinglePhase: {
        SunSpecInverterSinglePhaseModel *inverter = qobject_cast<SunSpecInverterSinglePhaseModel *>(model);
        //qCDebug(dcSunSpec()) << "Inverter block updated for" << thing->name();
        if (thing->thingClassId() == sunspecSinglePhaseInverterThingClassId) {
            thing->setStateValue(sunspecSinglePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecSinglePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            // Note: solar edge needs some calculations for the current pv power
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecSinglePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecSinglePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecSinglePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecSinglePhaseInverterPhaseVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecSinglePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecSinglePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecSinglePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecSinglePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        } else if (thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId) {
            thing->setStateValue(sunspecLimitableSinglePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecLimitableSinglePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            // Note: solar edge needs some calculations for the current pv power
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecLimitableSinglePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterPhaseVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecLimitableSinglePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecLimitableSinglePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        }
        break;
    }
    case SunSpecModelFactory::ModelIdInverterSinglePhaseFloat: {
        SunSpecInverterSinglePhaseFloatModel *inverter = qobject_cast<SunSpecInverterSinglePhaseFloatModel *>(model);
        //qCDebug(dcSunSpec()) << "Inverter block updated for" << thing->name();
        if (thing->thingClassId() == sunspecSinglePhaseInverterThingClassId) {
            thing->setStateValue(sunspecSinglePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecSinglePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            // Note: solar edge needs some calculations for the current pv power
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecSinglePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecSinglePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecSinglePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecSinglePhaseInverterPhaseVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecSinglePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecSinglePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecSinglePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecSinglePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecSinglePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        } else if (thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId) {
            thing->setStateValue(sunspecLimitableSinglePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecLimitableSinglePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            // Note: solar edge needs some calculations for the current pv power
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecLimitableSinglePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterPhaseVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecLimitableSinglePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecLimitableSinglePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecLimitableSinglePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        }
        break;
    }
    case SunSpecModelFactory::ModelIdInverterSplitPhase: {
        SunSpecInverterSplitPhaseModel *inverter = qobject_cast<SunSpecInverterSplitPhaseModel *>(model);
        //qCDebug(dcSunSpec()) << "Inverter block updated for" << thing->name();
        if (thing->thingClassId() == sunspecSplitPhaseInverterThingClassId) {
            thing->setStateValue(sunspecSplitPhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecSplitPhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecSplitPhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecSplitPhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecSplitPhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecSplitPhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecSplitPhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecSplitPhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecSplitPhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        } else if (thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId) {
            thing->setStateValue(sunspecLimitableSplitPhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecLimitableSplitPhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecLimitableSplitPhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecLimitableSplitPhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecLimitableSplitPhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        }
        break;
    }
    case SunSpecModelFactory::ModelIdInverterSplitPhaseFloat: {
        SunSpecInverterSplitPhaseFloatModel *inverter = qobject_cast<SunSpecInverterSplitPhaseFloatModel *>(model);
        //qCDebug(dcSunSpec()) << "Inverter block updated for" << thing->name();
        if (thing->thingClassId() == sunspecSplitPhaseInverterThingClassId) {
            thing->setStateValue(sunspecSplitPhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecSplitPhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecSplitPhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecSplitPhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecSplitPhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecSplitPhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecSplitPhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecSplitPhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecSplitPhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecSplitPhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecSplitPhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        } else if (thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId) {
            thing->setStateValue(sunspecLimitableSplitPhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecLimitableSplitPhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecLimitableSplitPhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecLimitableSplitPhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecLimitableSplitPhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecLimitableSplitPhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        }
        break;
    }
    case SunSpecModelFactory::ModelIdInverterThreePhase: {
        SunSpecInverterThreePhaseModel *inverter = qobject_cast<SunSpecInverterThreePhaseModel *>(model);
        //qCDebug(dcSunSpec()) << "Inverter block updated for" << thing->name();
        if (thing->thingClassId() == sunspecThreePhaseInverterThingClassId) {
            thing->setStateValue(sunspecThreePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecThreePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecThreePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecThreePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecThreePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecThreePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecThreePhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecThreePhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecThreePhaseInverterPhaseCNVoltageStateTypeId, inverter->phaseVoltageCn());
            thing->setStateValue(sunspecThreePhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecThreePhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecThreePhaseInverterPhaseCCurrentStateTypeId, inverter->ampsPhaseC());
            thing->setStateValue(sunspecThreePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecThreePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecThreePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecThreePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecThreePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        } else if (thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
            thing->setStateValue(sunspecLimitableThreePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecLimitableThreePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecLimitableThreePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecLimitableThreePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseCNVoltageStateTypeId, inverter->phaseVoltageCn());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseCCurrentStateTypeId, inverter->ampsPhaseC());
            thing->setStateValue(sunspecLimitableThreePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecLimitableThreePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecLimitableThreePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        }
        break;
    }
    case SunSpecModelFactory::ModelIdInverterThreePhaseFloat: {
        SunSpecInverterThreePhaseFloatModel *inverter = qobject_cast<SunSpecInverterThreePhaseFloatModel *>(model);
        //qCDebug(dcSunSpec()) << "Inverter block updated for" << thing->name();
        if (thing->thingClassId() == sunspecThreePhaseInverterThingClassId) {
            thing->setStateValue(sunspecThreePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecThreePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecThreePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecThreePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecThreePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecThreePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecThreePhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecThreePhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecThreePhaseInverterPhaseCNVoltageStateTypeId, inverter->phaseVoltageCn());
            thing->setStateValue(sunspecThreePhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecThreePhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecThreePhaseInverterPhaseCCurrentStateTypeId, inverter->ampsPhaseC());
            thing->setStateValue(sunspecThreePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecThreePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecThreePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecThreePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecThreePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        } else if (thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
            thing->setStateValue(sunspecLimitableThreePhaseInverterConnectedStateTypeId, true);
            thing->setStateValue(sunspecLimitableThreePhaseInverterVersionStateTypeId, model->commonModelInfo().versionString);
            double currentPower = calculateSolarEdgePvProduction(thing, -inverter->watts(), -inverter->dcWatts());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentPowerStateTypeId, currentPower);
            evaluateEnergyProducedValue(thing, inverter->wattHours() / 1000.0);
            thing->setStateValue(sunspecLimitableThreePhaseInverterTotalCurrentStateTypeId, inverter->amps());
            thing->setStateValue(sunspecLimitableThreePhaseInverterFrequencyStateTypeId, inverter->hz());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCabinetTemperatureStateTypeId, inverter->cabinetTemperature());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseANVoltageStateTypeId, inverter->phaseVoltageAn());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseBNVoltageStateTypeId, inverter->phaseVoltageBn());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseCNVoltageStateTypeId, inverter->phaseVoltageCn());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseACurrentStateTypeId, inverter->ampsPhaseA());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseBCurrentStateTypeId, inverter->ampsPhaseB());
            thing->setStateValue(sunspecLimitableThreePhaseInverterPhaseCCurrentStateTypeId, inverter->ampsPhaseC());
            thing->setStateValue(sunspecLimitableThreePhaseInverterOperatingStateStateTypeId, getInverterStateString(inverter->operatingState()));
            thing->setStateValue(sunspecLimitableThreePhaseInverterErrorStateTypeId, getInverterErrorString(inverter->event1()));
            thing->setStateValue(sunspecLimitableThreePhaseInverterVoltageDcStateTypeId, inverter->dcVoltage());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentDcStateTypeId, inverter->dcAmps());
            thing->setStateValue(sunspecLimitableThreePhaseInverterCurrentPowerDcStateTypeId, -inverter->dcWatts());
        }
        break;
    }
    default:
        qCWarning(dcSunSpec()) << "Received block data from unhandled model" << model;
        break;
    }
}

void IntegrationPluginSunSpec::onMeterBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecMeters.key(model);
    if (!thing) return;

    // Get parent thing
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing)
        return;

    switch (model->modelId()) {
    case SunSpecModelFactory::ModelIdMeterSinglePhase: {
        SunSpecMeterSinglePhaseModel *meter = qobject_cast<SunSpecMeterSinglePhaseModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecSinglePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPhaseAStateTypeId, -meter->ampsPhaseA());
        thing->setStateValue(sunspecSinglePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSinglePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSinglePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterSinglePhaseFloat: {
        SunSpecMeterSinglePhaseFloatModel *meter = qobject_cast<SunSpecMeterSinglePhaseFloatModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecSinglePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSinglePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->watts()));
        thing->setStateValue(sunspecSinglePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSinglePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSinglePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseAbn: {
        SunSpecMeterSplitSinglePhaseAbnModel *meter = qobject_cast<SunSpecMeterSplitSinglePhaseAbnModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecSplitPhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecSplitPhaseMeterTotalCurrentStateTypeId, fixValueSign(meter->amps(), -meter->watts()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseAStateTypeId, -meter->wattsPhaseA());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseBStateTypeId, -meter->wattsPhaseB());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->wattsPhaseA()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), -meter->wattsPhaseB()));
        thing->setStateValue(sunspecSplitPhaseMeterLnACVoltageStateTypeId, meter->voltageLn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecSplitPhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSplitPhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterSplitSinglePhaseFloat: {
        SunSpecMeterSplitSinglePhaseFloatModel *meter = qobject_cast<SunSpecMeterSplitSinglePhaseFloatModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecSplitPhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecSplitPhaseMeterTotalCurrentStateTypeId, fixValueSign(meter->amps(), -meter->watts()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseAStateTypeId, -meter->wattsPhaseA());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPowerPhaseBStateTypeId, -meter->wattsPhaseB());
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->wattsPhaseA()));
        thing->setStateValue(sunspecSplitPhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), -meter->wattsPhaseB()));
        thing->setStateValue(sunspecSplitPhaseMeterLnACVoltageStateTypeId, meter->voltageLn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecSplitPhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecSplitPhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecSplitPhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterThreePhase: {
        SunSpecMeterThreePhaseModel *meter = qobject_cast<SunSpecMeterThreePhaseModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, -meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, -meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, -meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), -meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), -meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdDeltaConnectThreePhaseAbcMeter: {
        SunSpecDeltaConnectThreePhaseAbcMeterModel *meter = qobject_cast<SunSpecDeltaConnectThreePhaseAbcMeterModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, -meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, -meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, -meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), -meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), -meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterThreePhaseWyeConnect: {
        SunSpecMeterThreePhaseWyeConnectModel *meter = qobject_cast<SunSpecMeterThreePhaseWyeConnectModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, -meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, -meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, -meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), -meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), -meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    case SunSpecModelFactory::ModelIdMeterThreePhaseDeltaConnect: {
        SunSpecMeterThreePhaseDeltaConnectModel *meter = qobject_cast<SunSpecMeterThreePhaseDeltaConnectModel *>(model);
        //qCDebug(dcSunSpec()) << "Meter block updated for" << thing->name();
        thing->setStateValue(sunspecThreePhaseMeterConnectedStateTypeId, true);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyProducedStateTypeId, meter->totalWattHoursExported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterTotalEnergyConsumedStateTypeId, meter->totalWattHoursImported() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseAStateTypeId, meter->totalWattHoursImportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseBStateTypeId, meter->totalWattHoursImportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyConsumedPhaseCStateTypeId, meter->totalWattHoursImportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseAStateTypeId, meter->totalWattHoursExportedPhaseA() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseBStateTypeId, meter->totalWattHoursExportedPhaseB() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterEnergyProducedPhaseCStateTypeId, meter->totalWattHoursExportedPhaseC() / 1000.0);
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerStateTypeId, -meter->watts());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseAStateTypeId, -meter->wattsPhaseA());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseBStateTypeId, -meter->wattsPhaseB());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPowerPhaseCStateTypeId, -meter->wattsPhaseC());
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseAStateTypeId, fixValueSign(meter->ampsPhaseA(), -meter->wattsPhaseA()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseBStateTypeId, fixValueSign(meter->ampsPhaseB(), -meter->wattsPhaseB()));
        thing->setStateValue(sunspecThreePhaseMeterCurrentPhaseCStateTypeId, fixValueSign(meter->ampsPhaseC(), -meter->wattsPhaseC()));
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseAStateTypeId, meter->phaseVoltageAn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseBStateTypeId, meter->phaseVoltageBn());
        thing->setStateValue(sunspecThreePhaseMeterVoltagePhaseCStateTypeId, meter->phaseVoltageCn());
        thing->setStateValue(sunspecThreePhaseMeterFrequencyStateTypeId, meter->hz());
        thing->setStateValue(sunspecThreePhaseMeterVersionStateTypeId, model->commonModelInfo().versionString);
        break;
    }
    default:
        qCWarning(dcSunSpec()) << "Received block data from unhandled model" << model;
        break;
    }
}

void IntegrationPluginSunSpec::onStorageBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecStorages.key(model);
    if (!thing) return;

    // #TODO needed in on<X>BlockUpdated methods?
    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) { return; }

    if (model->modelId() != SunSpecModelFactory::ModelIdStorage) {
        qCWarning(dcSunSpec()) << "onStorageBlockUpdated for model with model id != ModelIdStorage for thing" << thing->name();
        return;
    }

    //qCDebug(dcSunSpec()) << "Storage block updated for thing" << thing->name();
    SunSpecStorageModel *storage = qobject_cast<SunSpecStorageModel *>(model);
    auto storageStatus = QString{};
    auto chargingState = QString{};
    switch (storage->chaSt()) {
    case SunSpecStorageModel::ChastOff:
        storageStatus = "Off";
        chargingState = "idle";
        break;
    case SunSpecStorageModel::ChastEmpty:
        storageStatus = "Empty";
        chargingState = "idle";
        break;
    case SunSpecStorageModel::ChastDischarging:
        storageStatus = "Discharging";
        chargingState = "discharging";
        break;
    case SunSpecStorageModel::ChastCharging:
        storageStatus = "Charging";
        chargingState = "charging";
        break;
    case SunSpecStorageModel::ChastFull:
        storageStatus = "Full";
        chargingState = "idle";
        break;
    case SunSpecStorageModel::ChastHolding:
        storageStatus = "Holding";
        chargingState = "idle";
        break;
    case SunSpecStorageModel::ChastTesting:
        storageStatus = "Testing";
        chargingState = "idle";
        break;
    }

    if (thing->thingClassId() == sunspecStorageThingClassId) {
        thing->setStateValue(sunspecStorageConnectedStateTypeId, true);
        thing->setStateValue(sunspecStorageVersionStateTypeId, model->commonModelInfo().versionString);
        thing->setStateValue(sunspecStorageBatteryCriticalStateTypeId, storage->chaState() < 5);
        thing->setStateValue(sunspecStorageBatteryLevelStateTypeId, qRound(storage->chaState()));
        thing->setStateValue(sunspecStorageGridChargingStateTypeId, storage->chaGriSet() == SunSpecStorageModel::ChagrisetGrid);
        thing->setStateValue(sunspecStorageEnableChargingStateTypeId, storage->storCtlMod().testFlag(SunSpecStorageModel::Storctl_modCharge));
        thing->setStateValue(sunspecStorageChargingRateStateTypeId, storage->wChaGra());
        thing->setStateValue(sunspecStorageDischargingRateStateTypeId, storage->wDisChaGra());
        thing->setStateValue(sunspecStorageStorageStatusStateTypeId, storageStatus);
        thing->setStateValue(sunspecStorageChargingStateStateTypeId, chargingState);
    } else if (thing->thingClassId() == froniusControllableStorageThingClassId) {
        thing->setStateValue(froniusControllableStorageConnectedStateTypeId, true);
        const auto soc = storage->chaState();
        const auto socInt = qRound(soc);
        thing->setStateValue(froniusControllableStorageBatteryCriticalStateTypeId, socInt < 5);
        thing->setStateValue(froniusControllableStorageBatteryLevelStateTypeId, socInt);
        thing->setStateValue(froniusControllableStorageChargingStateStateTypeId, chargingState);
        thing->setStateValue(froniusControllableStorageStorageStatusStateTypeId, storageStatus);
        const auto isForcePowerEnabled =
                storage->storCtlMod().testFlag(SunSpecStorageModel::Storctl_modCharge) &&
                storage->storCtlMod().testFlag(SunSpecStorageModel::Storctl_modDiScharge);
        thing->setStateValue(froniusControllableStorageEnableForcePowerStateStateTypeId, isForcePowerEnabled);
        thing->setStateValue(froniusControllableStorageVersionStateTypeId, model->commonModelInfo().versionString);
        thing->setStateMinMaxValues(froniusControllableStorageForcePowerStateTypeId,
                                    -storage->wChaMax(),
                                    storage->wChaMax());
        thing->setStateValue(froniusControllableStorageInWRteFeedbackStateTypeId, storage->inWRte());
        thing->setStateValue(froniusControllableStorageOutWRteFeedbackStateTypeId, storage->outWRte());
        auto storCtlModStr = QString{};
        const auto storCtlMod = storage->storCtlMod();
        if (storCtlMod.testFlag(SunSpecStorageModel::Storctl_modCharge) &&
                storCtlMod.testFlag(SunSpecStorageModel::Storctl_modDiScharge))
        {
            storCtlModStr = "Both";
        } else if (storCtlMod.testFlag(SunSpecStorageModel::Storctl_modCharge)) {
            storCtlModStr = "Charge";
        } else if (storCtlMod.testFlag(SunSpecStorageModel::Storctl_modDiScharge)) {
            storCtlModStr = "Discharge";
        } else {
            storCtlModStr = "None";
        }
        thing->setStateValue(froniusControllableStorageStorCtl_ModStateTypeId, storCtlModStr);

        const auto forcePowerEnabled = thing->stateValue(froniusControllableStorageEnableForcePowerStateTypeId).toBool();
        if (!forcePowerEnabled) {
            const auto maxSoCSetpoint = thing->stateValue(froniusControllableStorageSetMaxSoCStateTypeId).toInt();
            if (soc > maxSoCSetpoint) {
                setChargingAllowed(thing, false, nullptr);
            } else {
                setChargingAllowed(thing, true, nullptr);
            }
        }
    }
}

void IntegrationPluginSunSpec::onSolarEdgeBatteryBlockUpdated()
{
    SolarEdgeBattery *battery = qobject_cast<SolarEdgeBattery *>(sender());
    Thing *thing = battery->thing();

    qCDebug(dcSunSpec()) << "SolarEdgeBattery: block data updated";// << battery->batteryData();

    thing->setStateValue(solarEdgeBatteryConnectedStateTypeId, true);

    QString chargingState = "idle";
    switch (battery->batteryData().batteryStatus) {
    case SolarEdgeBattery::Off:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Off");
        break;
    case SolarEdgeBattery::Standby:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Standby");
        break;
    case SolarEdgeBattery::Init:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Init");
        break;
    case SolarEdgeBattery::Charge:
        chargingState = "charging";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Charging");
        break;
    case SolarEdgeBattery::Discharge:
        chargingState = "discharging";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Discharging");
        break;
    case SolarEdgeBattery::Fault:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Fault");
        break;
    case SolarEdgeBattery::Holding:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Holding");
        break;
    case SolarEdgeBattery::Idle:
        chargingState = "idle";
        thing->setStateValue(solarEdgeBatteryBatteryStatusStateTypeId, "Idle");
        break;
    }

    thing->setStateValue(solarEdgeBatteryBatteryCriticalStateTypeId, (battery->batteryData().stateOfEnergy < 5) && chargingState != "charging");
    thing->setStateValue(solarEdgeBatteryBatteryLevelStateTypeId, battery->batteryData().stateOfEnergy);
    thing->setStateValue(solarEdgeBatteryChargingStateStateTypeId, chargingState);
    thing->setStateValue(solarEdgeBatteryRatedEnergyStateTypeId, battery->batteryData().ratedEnergy / 1000.0); // kWh
    thing->setStateValue(solarEdgeBatteryAverageTemperatureStateTypeId, battery->batteryData().averageTemperature);
    thing->setStateValue(solarEdgeBatteryInstantaneousVoltageStateTypeId, battery->batteryData().instantaneousVoltage);
    thing->setStateValue(solarEdgeBatteryInstantaneousCurrentStateTypeId, battery->batteryData().instantaneousCurrent);
    thing->setStateValue(solarEdgeBatteryCurrentPowerStateTypeId, battery->batteryData().instantaneousPower);
    thing->setStateValue(solarEdgeBatteryMaxEnergyStateTypeId, battery->batteryData().maxEnergy / 1000.0); // kWh
    thing->setStateValue(solarEdgeBatteryCapacityStateTypeId, battery->batteryData().availableEnergy / 1000.0); // kWh
    thing->setStateValue(solarEdgeBatteryStateOfHealthStateTypeId, battery->batteryData().stateOfHealth);
    thing->setStateValue(solarEdgeBatteryVersionStateTypeId, battery->batteryData().firmwareVersion);

    thing->setStateValue(solarEdgeBatteryEnableForcePowerStateStateTypeId, battery->batteryData().storageControlMode == 4);
}

void IntegrationPluginSunSpec::onSettingsBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecSettings.key(model);
    if (!thing) { return; }

    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) { return; }

    if (model->modelId() != SunSpecModelFactory::ModelIdSettings) {
        qCWarning(dcSunSpec()) << "onSettingsBlockUpdated for model with model id != ModelIdSettings for thing" << thing->name();
        return;
    }

    //qCDebug(dcSunSpec()) << "Settings block updated for thing" << thing->name();
    if (thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId ||
            thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId ||
            thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
        SunSpecSettingsModel *settingsModel = qobject_cast<SunSpecSettingsModel *>(model);
        thing->setStateValue("maxPowerOutput", settingsModel->wMax());
    }
}

void IntegrationPluginSunSpec::onControlsBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecControls.key(model);
    if (!thing) { return; }

    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) { return; }

    if (model->modelId() != SunSpecModelFactory::ModelIdControls) {
        qCWarning(dcSunSpec()) << "onControlsBlockUpdated for model with model id != ModelIdControls for thing" << thing->name();
        return;
    }

    //qCDebug(dcSunSpec()) << "Controls block data updated for" << thing->name();
    if (thing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId ||
            thing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId ||
            thing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
        SunSpecControlsModel *controlsModel = qobject_cast<SunSpecControlsModel *>(model);
        //qCDebug(dcSunSpec()) << "\tWMaxLimEna:" << controlsModel->wMaxLimEna();
        //qCDebug(dcSunSpec()) << "\tWMaxLimPct:" << controlsModel->wMaxLimPct() << "%";
        thing->setStateValue("enableExportLimit",
                             controlsModel->wMaxLimEna() == SunSpecControlsModel::Wmaxlim_enaEnabled);
        const auto exportLimitWatt = controlsModel->wMaxLimPct() * thing->stateValue("maxPowerOutput").toFloat() / 100.f;
        thing->setStateValue("exportLimitFeedback", exportLimitWatt);
    }
}

void IntegrationPluginSunSpec::onNameplateBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecNameplates.key(model);
    if (!thing) return;

    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) { return; }

    if (model->modelId() != SunSpecModelFactory::ModelIdNameplate) {
        qCWarning(dcSunSpec()) << "onNameplateBlockUpdated for model with model id != ModelIdNameplate for thing" << thing->name();
        return;
    }

    //qCDebug(dcSunSpec()) << "Nameplate block data updated for" << thing->name();
    if (thing->thingClassId() == froniusControllableStorageThingClassId) {
        SunSpecNameplateModel *nameplate = qobject_cast<SunSpecNameplateModel *>(model);
        thing->setStateValue(froniusControllableStorageCapacityStateTypeId, nameplate->whRtg() / 1000);
    }
}

void IntegrationPluginSunSpec::onMpptBlockUpdated()
{
    SunSpecModel *model = qobject_cast<SunSpecModel *>(sender());
    Thing *thing = m_sunSpecMppts.key(model);
    if (!thing) return;

    Thing *parentThing = myThings().findById(thing->parentId());
    if (!parentThing) { return; }

    if (model->modelId() != SunSpecModelFactory::ModelIdMppt) {
        qCWarning(dcSunSpec()) << "onMpptBlockUpdated for model with model id != ModelIdMppt for thing" << thing->name();
        return;
    }

    //qCDebug(dcSunSpec()) << "MPPT block data updated for" << thing->name();
    const auto mppt = qobject_cast<SunSpecMpptModel *>(model);
    if (!mppt) {
        qCWarning(dcSunSpec()) << "Expected MPPT model but got other model";
        return;
    }

    const auto repeatingBlocks = mppt->repeatingBlocks();
    auto currentPower = 0.f;
    foreach (SunSpecModelRepeatingBlock *block, repeatingBlocks) {
        const auto mpptBlock = qobject_cast<SunSpecMpptModelRepeatingBlock *>(block);
        if (!mpptBlock) {
            qCWarning(dcSunSpec()) << "Unable to get SunSpecMpptModelRepeatingBlock from models' repeating blocks";
            continue;
        }
        if (mpptBlock->inputIdSting() == "StCha 3") {
            if (!qFuzzyIsNull(mpptBlock->dcPower())) {
                currentPower = mpptBlock->dcPower();
            }
        } else if (mpptBlock->inputIdSting() == "StDisCha 4") {
            if (!qFuzzyIsNull(mpptBlock->dcPower())) {
                currentPower = -mpptBlock->dcPower(); // Discharging is negative by convention
            }
        }
    }
    thing->setStateValue(froniusControllableStorageCurrentPowerStateTypeId, currentPower);
}

void IntegrationPluginSunSpec::evaluateEnergyProducedValue(Thing *inverterThing, float energyProduced)
{
    /* Note: on some systems the inverter sends for a longer period an absurdly
     * high and wrong value for the produced energy (seen so far with SolarEdge inverters).
     *
     * In order to catch such situations, we need to verify if the state changed makes sense,
     * or if the difference is to big for a regular produced energy value.
     *
     * Following scenarios need to be considered:
     * - This is the first data value, we have no history to verify if this values makes sense
     * - The system might be switched off for some time, the energy produced could be much more than the last known value
     * - More than one value in a row could occure, not only single garbage data value
     */

    StateTypeId energyProducedStateTypeId;
    if (inverterThing->thingClassId() == sunspecSinglePhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecSinglePhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecSplitPhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecSplitPhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecThreePhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecThreePhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecLimitableSinglePhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecLimitableSinglePhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecLimitableSplitPhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecLimitableSplitPhaseInverterTotalEnergyProducedStateTypeId;
    } else if (inverterThing->thingClassId() == sunspecLimitableThreePhaseInverterThingClassId) {
        energyProducedStateTypeId = sunspecLimitableThreePhaseInverterTotalEnergyProducedStateTypeId;
    } else {
        qCWarning(dcSunSpec()) << "Could not evaluate energy produced value for ThingClassId" << inverterThing->thingClassId() << "The value will not be updated.";
        return;
    }

    double currentEnergyValue = inverterThing->stateValue(energyProducedStateTypeId).toDouble();
    if (currentEnergyValue <= 0) {
        // Probably the initial value, no fancy data handling here
        inverterThing->setStateValue(energyProducedStateTypeId, energyProduced);
    } else {
        double producedDiff = energyProduced - currentEnergyValue;
        if (producedDiff > 10000 /*kWh*/) {
            // The new energy value is way to high in order to be a reglar energy produced change...
            qCWarning(dcSunSpec()) << "The energy produced value for" << inverterThing << "is way to high compared to the previouse value:"
                                   << currentEnergyValue << "kWh. Ignoring the value:" << energyProduced << "kWh.";
            return;
        } else {
            // Not a huge jump, just set the value
            inverterThing->setStateValue(energyProducedStateTypeId, energyProduced);
        }
    }
}
