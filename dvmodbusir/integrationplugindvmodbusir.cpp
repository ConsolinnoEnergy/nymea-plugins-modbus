/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2024 Consolinno Energy GmbH <info@consolinno.de>                 *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation;                  *
 *  version 3 of the License.                                              *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationplugindvmodbusir.h"
#include "plugininfo.h"
#include <hardwaremanager.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>

#include <types/param.h>

#include <QString>
#include <QEventLoop>
#include <QtMath>

IntegrationPluginDvModbusIR::IntegrationPluginDvModbusIR()
{
}

/**
 * Initialize the plugin, nothing is done
 *  
 * @param nothing
 * @return nothing
 */
void IntegrationPluginDvModbusIR::init()
{
    // Initialisation can be done here.
    qCDebug(dcDvModbusIR()) << "Initialize plugin.";
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid) {
        qCDebug(dcDvModbusIR()) << "Modbus RTU master has been removed" << modbusUuid.toString();
        foreach (Thing *thing, myThings()) {
            if (thing->thingClassId() == dvModbusIRThingClassId) {
                if (thing->paramValue(dvModbusIRThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    qCWarning(dcDvModbusIR()) << "Modbus RTU hardware resource removed for" << thing <<". The thing will not be functional anymore.";
                    thing->setStateValue(dvModbusIRConnectedStateTypeId, false);
                    delete m_rtuConnections.take(thing);
                }
            }
        }
    });
}

/**
 * Discovery of dvModbusIR adapters
 * As the Modbus address is set for each device, the address is taken from user input.
 * This function searches for a configured modbus master and sets infos of the thing.
 *  
 * @param info provided by nymea
 * @return nothing
 */
void IntegrationPluginDvModbusIR::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == dvModbusIRThingClassId)
    {
        qCDebug(dcDvModbusIR()) << "Discovering modbus RTU resources...";

        QList<ModbusRtuMaster*> candidateMasters;
        foreach(ModbusRtuMaster *master, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) 
        {
            if (master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::EvenParity && master->baudrate() == 19200)
                candidateMasters.append(master);
        }
        if (candidateMasters.isEmpty())
        {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
        }
        qCInfo(dcDvModbusIR()) << "Discovery: Searching for DvModbusIR on modbus RTU...";
        uint modbusAddress = info->params().paramValue(dvModbusIRDiscoveryModbusIdParamTypeId).toUInt();
        if (modbusAddress > 254 || modbusAddress == 0)
        {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus address must be a value between 1 and 254."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, candidateMasters)
        {
            qCDebug(dcDvModbusIR()) << "Found RTU master resource " << modbusMaster << " connected " << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ModbusRtuReply *reply = modbusMaster->readHoldingRegister(modbusAddress, 0, 1);
            connect(reply, &ModbusRtuReply::finished, this, [=]() {
                qCDebug(dcDvModbusIR()) << "Test reply finished!" << reply->error() << reply->result();
                if (reply->error() == ModbusRtuReply::NoError && reply->result().length() > 0) {
                    quint16 serialNumber = reply->result().first();
                    qCDebug(dcDvModbusIR()) << "Read serialNumber is" << serialNumber;
                    
                    if (serialNumber == modbusAddress) {
                        ThingDescriptor descriptor(info->thingClassId(), "dvModbusIR", QString::number(modbusAddress) + " " + modbusMaster->serialPort());
                        ParamList params;
                        params << Param(dvModbusIRThingModbusIdParamTypeId, modbusAddress);
                        params << Param(dvModbusIRThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
                        params << Param(dvModbusIRThingSerialNumberParamTypeId, serialNumber);
                        descriptor.setParams(params);

                        // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                        // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                        // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                        Things existingThings = myThings().filterByThingClassId(dvModbusIRThingClassId).filterByParam(dvModbusIRThingSerialNumberParamTypeId, serialNumber);
                        if (!existingThings.isEmpty()) {
                            descriptor.setThingId(existingThings.first()->id());
                        }

                        info->addThingDescriptor(descriptor);
                    } else {
                        qCWarning(dcDvModbusIR()) << "Read serial number is" << serialNumber << "while the modbus address is" << modbusAddress;
                    }
                }
            });
        }
        info->finish(Thing::ThingErrorNoError);
    }
}

/**
 * Setup the plugin.
 * Create ModbusRtu connection and assign it to the thing.
 * Get the serial number and device status of the dvModbusIR adapter.
 * Additionally, get the number of the meter it is connected to.
 * Afterwards, setup the connections.
 *  
 * @param info provided by nymea
 * @return nothing
 */
void IntegrationPluginDvModbusIR::setupThing(ThingSetupInfo *info)
{
    qCDebug(dcDvModbusIR()) << "Setup thing" << info->thing();
    Thing *thing = info->thing();
    // A thing is being set up. Use info->thing() to get details of the thing, do
    // the required setup (e.g. connect to the device) and call info->finish() when done.

    if (thing->thingClassId() == dvModbusIRThingClassId)
    {
        qCDebug(dcDvModbusIR()) << "Setting up dvModbusIR";

        uint address = thing->paramValue(dvModbusIRThingModbusIdParamTypeId).toUInt();

        // Get Modbus master uuid
        QUuid uuid = thing->paramValue(dvModbusIRThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid))
        {
            qCWarning(dcDvModbusIR()) << "Setup faild, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing))
        {
            qCDebug(dcDvModbusIR()) << "Already have a connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        // Create modbus rtu connection
        DvModbusIRModbusRtuConnection *connection = new DvModbusIRModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        // Try the reachable check five times, as it might not working at first, even though the modbus connection is working
        connection->setCheckReachableRetries(5);

        connect(connection, &DvModbusIRModbusRtuConnection::reachableChanged, this, [=](bool reachable) {
            // If the device is reachable, set connected state
            thing->setStateValue(dvModbusIRConnectedStateTypeId, reachable);
            if (reachable)
            {
                connection->initialize();
                qCDebug(dcDvModbusIR()) << "Device " << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcDvModbusIR()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
                thing->setStateValue(dvModbusIRCurrentPowerStateTypeId, 0);
            }
        });

        connect(connection, &DvModbusIRModbusRtuConnection::initializationFinished, thing, [=](bool success) {
            // If the communication is working, get general info about the connected devices
            if (success)
            {
                qCDebug(dcDvModbusIR()) << "dvModbusIR initialization successful.";
                thing->setStateValue(dvModbusIRSerialNumberStateTypeId, connection->serialNumber());
                thing->setStateValue(dvModbusIRDeviceStatusStateTypeId, connection->deviceStatus());
                thing->setStateValue(dvModbusIRMeterIdStateTypeId, connection->meterId());
                uint meterDataValue = connection->meterData();
                thing->setStateValue(dvModbusIRMeterDataStateTypeId, QString::number(meterDataValue,2));
                // Set the name of the thing to "DvIR-<meter number>"
                thing->setName("DvIR-"+thing->stateValue(dvModbusIRMeterIdStateTypeId).toString());
            } else {
                qCWarning(dcDvModbusIR()) << "dvModbusIR initialization failed.";
                connection->initialize();
            }
        });

        connect(connection, &DvModbusIRModbusRtuConnection::updateFinished, this, [thing, connection]() {
            qCDebug(dcDvModbusIR()) << "dvModbusIR - Update finished";

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set produced energy";
            qint16 prodEnergyExpReg = connection->producedEnergyExponent();
            quint64 prodEngReg = connection->totalProducedEnergy();
            double producedEnergy = prodEngReg*qPow(10, prodEnergyExpReg-3);
            thing->setStateValue(dvModbusIRTotalEnergyProducedStateTypeId, producedEnergy);

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set produced energy Tarif 1";
            qint16 prodEnergyExpRegTarif1 = connection->producedEnergyExponentTarif1();
            quint64 prodEngRegTarif1 = connection->totalProducedEnergyTarif1();
            double producedEnergyTarif1 = prodEngRegTarif1*qPow(10, prodEnergyExpRegTarif1-3);
            thing->setStateValue(dvModbusIRTotalEnergyProducedTarif1StateTypeId, producedEnergyTarif2);

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set produced energy Tarif 2";
            qint16 prodEnergyExpRegTarif2 = connection->producedEnergyExponentTarif2();
            quint64 prodEngRegTarif2 = connection->totalProducedEnergyTarif2();
            double producedEnergyTarif2 = prodEngRegTarif2*qPow(10, prodEnergyExpRegTarif2-3);
            thing->setStateValue(dvModbusIRTotalEnergyProducedTarif2StateTypeId, producedEnergyTarif2);

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set consumed energy";
            qint16 consEnergyExpReg = connection->consumedEnergyExponent();
            quint64 consEngReg = connection->totalConsumedEnergy();
            double consumedEnergy = consEngReg*qPow(10, consEnergyExpReg-3);
            thing->setStateValue(dvModbusIRTotalEnergyConsumedStateTypeId, consumedEnergy);

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set consumed energy Tarif 1";
            qint16 consEnergyExpRegTarif1 = connection->consumedEnergyExponentTarif1();
            quint64 consEngRegTarif1 = connection->totalConsumedEnergyTarif1();
            double consumedEnergyTarif1 = consEngRegTarif1*qPow(10, consEnergyExpRegTarif1-3);
            thing->setStateValue(dvModbusIRTotalEnergyConsumedTarif1StateTypeId, consumedEnergyTarif1);

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set consumed energy Tarif 2";
            qint16 consEnergyExpRegTarif2 = connection->consumedEnergyExponentTarif2();
            quint64 consEngRegTarif2 = connection->totalConsumedEnergyTarif2();
            double consumedEnergyTarif2 = consEngRegTarif2*qPow(10, consEnergyExpRegTarif2-3);
            thing->setStateValue(dvModbusIRTotalEnergyConsumedTarif2StateTypeId, consumedEnergyTarif2);

            qCDebug(dcDvModbusIR()) << "dvModbusIR - Set current power";
            qint16 currentPowerExpReg = connection->currentPowerExponent();
            quint32 currentPowerReg = connection->currentPower();
            double currentPower = currentPowerReg*qPow(10, currentPowerExpReg);
            thing->setStateValue(dvModbusIRCurrentPowerStateTypeId, currentPower);
        });
        m_rtuConnections.insert(thing, connection);
    }
    info->finish(Thing::ThingErrorNoError);
}

/**
 * Executed after setup.
 * Setup the plugin timer. After 10min, get the current values from the connected meters
 *
 * @param thing being set up
 * @return nothing
 */
void IntegrationPluginDvModbusIR::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcDvModbusIR()) << "Post setup thing";

    if (!m_pluginTimer)
    {
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcDvModbusIR()) << "Update dvModbusIR";
            foreach(Thing *thing, myThings()) {
                if (thing->thingClassId() == dvModbusIRThingClassId)
                {
                    m_rtuConnections.value(thing)->update();
                }
             }
        });
    }
    m_pluginTimer->start();
}

/**
 * Executed when a thing is being removed.
 * If there are no things left, unregister pluginTimer
 *
 * @param thing to be removed
 * @return nothing
 */
void IntegrationPluginDvModbusIR::thingRemoved(Thing *thing)
{
    // A thing is being removed from the system. Do the required cleanup
    // (e.g. disconnect from the device) here.
    qCDebug(dcDvModbusIR()) << "Remove thing" << thing;

    if (m_rtuConnections.contains(thing))
        m_rtuConnections.take(thing)->deleteLater();

    if (myThings().isEmpty() && m_pluginTimer)
    {
        qCDebug(dcDvModbusIR()) << "Stopping update timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
