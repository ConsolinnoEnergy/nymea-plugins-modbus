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
 * 
 *  
 * @param nothing
 * @return nothing
 */
void IntegrationPluginDvModbusIR::init()
{
    // Initialisation can be done here.
    qCDebug(dcDvModbusIR()) << "Initialize plugin.";
}

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

            ThingDescriptor descriptor(info->thingClassId(), "dvModbusIR", QString::number(modbusAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(dvModbusIRThingModbusIdParamTypeId, modbusAddress);
            params << Param(dvModbusIRThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }
        info->finish(Thing::ThingErrorNoError);
    }
}

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

        DvModbusIRModbusRtuConnection *connection = new DvModbusIRModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connection->setCheckReachableRetries(5);

        connect(connection, &DvModbusIRModbusRtuConnection::reachableChanged, this, [=](bool reachable) {
            thing->setStateValue(dvModbusIRConnectedStateTypeId, reachable);
            if (reachable)
            {
                connection->initialize();
                qCDebug(dcDvModbusIR()) << "Device " << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcDvModbusIR()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });

        connect(connection, &DvModbusIRModbusRtuConnection::initializationFinished, thing, [=](bool success) {
            if (success)
            {
                qCDebug(dcDvModbusIR()) << "dvModbusIR initialization successful.";
                thing->setStateValue(dvModbusIRSerialNumberStateTypeId, connection->serialNumber());
                thing->setStateValue(dvModbusIRDeviceStatusStateTypeId, connection->deviceStatus());
                thing->setStateValue(dvModbusIRMeterIdStateTypeId, connection->meterId());
                thing->setName("DvIR-"+thing->stateValue(dvModbusIRMeterIdStateTypeId).toString());
            } else {
                qCWarning(dcDvModbusIR()) << "dvModbusIR initialization failed.";
                connection->initialize();
            }
        });

        connect(connection, &DvModbusIRModbusRtuConnection::totalProducedEnergyChanged, this, [=](quint64 totalProducedEnergy) {
            connect(connection, &DvModbusIRModbusRtuConnection::producedEnergyExponentChanged, this, [=](qint16 producedEnergyExponent) {
                thing->setStateValue(dvModbusIRTotalEnergyProducedStateTypeId, totalProducedEnergy*qPow(10, producedEnergyExponent-3));
            });
        });

        connect(connection, &DvModbusIRModbusRtuConnection::totalConsumedEnergyChanged, this, [=](quint64 totalConsumedEnergy) {
            connect(connection, &DvModbusIRModbusRtuConnection::consumedEnergyExponentChanged, this, [=](qint16 consumedEnergyExponent) {
                thing->setStateValue(dvModbusIRTotalEnergyConsumedStateTypeId, totalConsumedEnergy*qPow(10, consumedEnergyExponent-3));
            });
        });
        m_rtuConnections.insert(thing, connection);
    }
    info->finish(Thing::ThingErrorNoError);
}

/**
 * Executed after setup.
 * Setup the plugin timer. After 10min, get the current values from the connected meters
 *
 * @param [Thing*] thing: provided by nymea
 * @return nothing
 */
void IntegrationPluginDvModbusIR::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
        qCDebug(dcDvModbusIR()) << "Post setup thing";

    if (!m_pluginTimer)
    {
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10*60);
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

void IntegrationPluginDvModbusIR::thingRemoved(Thing *thing)
{
    // A thing is being removed from the system. Do the required cleanup
    // (e.g. disconnect from the device) here.
    qCDebug(dcDvModbusIR()) << "Remove thing" << thing;

    if (myThings().isEmpty() && m_pluginTimer)
    {
        qCDebug(dcDvModbusIR()) << "Stopping update timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
