#include "integrationplugingrowatt.h"
#include "plugininfo.h"
#include <hardwaremanager.h>
#include <networkdevicediscovery.h>
#include <modbus/modbusrtuconnection.h>
#include <serialport.h>

IntegrationPluginGrowatt::IntegrationPluginGrowatt()
{
}

void IntegrationPluginGrowatt::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=](const QUuid &modbusUuid){
        qCDebug(dcGrowatt()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(growattInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcGrowatt()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(growattInverterRTUConnectedStateTypeId, false);

                // Set connected state for meter
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(growattMeterConnectedStateTypeId, false);
                }

                // Set connected state for battery
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    foreach (Thing *batteryThing, batteryThings) {
                        batteryThing->setStateValue(growattBatteryConnectedStateTypeId, false);
                    }
                }

                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginGrowatt::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == growattInverterRTUThingClassId) {
        // Implement Modbus RTU-specific discovery if applicable
    }
}

void IntegrationPluginGrowatt::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcGrowatt()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == growattInverterRTUThingClassId) {
        uint address = thing->paramValue(growattInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address < 1) {
            qCWarning(dcGrowatt()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(growattInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcGrowatt()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcGrowatt()) << "Already have a Growatt connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        GrowattModbusRtuConnection *connection = new GrowattModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &GrowattModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(growattInverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterConnectedStateTypeId, reachable);
            }

            // Set connected state for battery
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                foreach (Thing *batteryThing, batteryThings) {
                    batteryThing->setStateValue(growattBatteryConnectedStateTypeId, reachable);
                }
            }

            if (reachable) {
                qCDebug(dcGrowatt()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcGrowatt()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });

        // Handle property changed signals
        connect(connection, &GrowattModbusRtuConnection::propertyChanged, this, [this, thing](const QString &property, const QVariant &value){
            qCDebug(dcGrowatt()) << "Inverter property" << property << "changed to" << value;
            thing->setStateValue(property, value);
        });

        // FIXME: make async and check if this is really a Growatt
        m_rtuConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        return;
    }

    if (thing->thingClassId() == growattMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(growattMeterConnectedStateTypeId, parentThing->stateValue(growattInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }

    if (thing->thingClassId() == growattBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(growattBatteryConnectedStateTypeId, parentThing->stateValue(growattInverterRTUConnectedStateTypeId).toBool());
            thing->setStateValue(growattBatteryCapacityStateTypeId, parentThing->paramValue(growattInverterRTUThingBatteryCapacityParamTypeId).toUInt());
        }
        return;
    }
}

void IntegrationPluginGrowatt::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == growattInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcGrowatt()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach (GrowattModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if we have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId).isEmpty()) {
            qCDebug(dcGrowatt()) << "Set up Growatt meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(growattMeterThingClassId, "Growatt Power Meter", QString(), thing->id()));
        }

        // Check if we have to set up a child battery for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(growattBatteryThingClassId).isEmpty()) {
            qCDebug(dcGrowatt()) << "Set up Growatt battery for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(growattBatteryThingClassId, "Growatt Battery", QString(), thing->id()));
        }
    }
}

void IntegrationPluginGrowatt::thingRemoved(Thing *thing)
{
    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginGrowatt::setInverterErrorMessage(Thing *thing, quint32 errorBits)
{
    QString errorMessage{""};
    if (errorBits == 0) {
        errorMessage.append("No error, everything ok.");
    } else {
        errorMessage.append("The following error bits are active: ");
        quint32 testBit{0b01};
        for (int var = 0; var < 32; ++var) {
            if (errorBits & (testBit << var)) {
                errorMessage.append(QStringLiteral("bit %1, ").arg(var));
                if (var == 0) {
                    errorMessage.append(" The GFCI detecting circuit is abnormal, ");
                } else if (var == 1) {
                    errorMessage.append(" The output current sensor is abnormal, ");
                } else if (var == 3) {
                    errorMessage.append(" Different DCI value on Master and Slave, ");
                } else if (var == 4) {
                    errorMessage.append(" Different GFCI values on Master & Slave, ");
                } else if (var == 6) {
                    errorMessage.append(" GFCI check failure 3 times, ");
                } else if (var == 7) {
                    errorMessage.append(" Relay check failure 3 times, ");
                } else if (var == 8) {
                    errorMessage.append(" AC HCT check failure 3 times, ");
                } else if (var == 9) {
                    errorMessage.append(" Utility is unavailable, ");
                } else if (var == 10) {
                    errorMessage.append(" Ground current is too high, ");
                } else if (var == 11) {
                    errorMessage.append(" Dc bus is too high, ");
                } else if (var == 12) {
                    errorMessage.append(" The fan in case failure, ");
                } else if (var == 13) {
                    errorMessage.append(" Temperature is too high, ");
                } else if (var == 14) {
                    errorMessage.append(" Utility Phase Failure, ");
                } else if (var == 15) {
                    errorMessage.append(" Pv input voltage is over the tolerable maximum value, ");
                } else if (var == 16) {
                    errorMessage.append(" The external fan failure, ");
                } else if (var == 17) {
                    errorMessage.append(" Grid voltage out of tolerable range, ");
                } else if (var == 18) {
                    errorMessage.append(" Isolation resistance of PV-plant too low, ");
                } else if (var == 19) {
                    errorMessage.append(" The DC injection to grid is too high, ");
                } else if (var == 20) {
                    errorMessage.append(" Back-Up Over Load, ");
                } else if (var == 22) {
                    errorMessage.append(" Different value between Master and Slave for grid frequency, ");
                } else if (var == 23) {
                    errorMessage.append(" Different value between Master and Slave for grid voltage, ");
                } else if (var == 25) {
                    errorMessage.append(" Relay check is failure, ");
                } else if (var == 27) {
                    errorMessage.append(" Phase angle out of range (110°~140°), ");
                } else if (var == 28) {
                    errorMessage.append(" Communication between ARM and DSP is failure, ");
                } else if (var == 29) {
                    errorMessage.append(" The grid frequency is out of tolerable range, ");
                } else if (var == 30) {
                    errorMessage.append(" EEPROM cannot be read or written, ");
                } else if (var == 31) {
                    errorMessage.append(" Communication between microcontrollers is failure, ");
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
    qCDebug(dcGrowatt()) << errorMessage;
    thing->setStateValue(growattInverterRTUErrorMessageStateTypeId, errorMessage);
}
