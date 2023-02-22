/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Tim Stöcker <t.stoecker@consolinno.de>                 *
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

#include "plugininfo.h"
#include "integrationpluginalphatec.h"
#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

IntegrationPluginAlphatec::IntegrationPluginAlphatec()
{

}

void IntegrationPluginAlphatec::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
          qCDebug(dcAlphatec()) << "Modbus RTU master has been removed" << modbusUuid.toString();

          foreach (Thing *thing, myThings()) {
              if (thing->paramValue(alphatecWallboxPowerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                  qCWarning(dcAlphatec()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                  thing->setStateValue(alphatecWallboxPowerConnectedStateTypeId, false);
                  delete m_rtuConnections.take(thing);
              }
          }
      });

}


void IntegrationPluginAlphatec::discoverThings(ThingDiscoveryInfo *info)
{

        qCDebug(dcAlphatec()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(alphatecWallboxPowerDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcAlphatec()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "Alphatec Wallbox", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(alphatecWallboxPowerThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(alphatecWallboxPowerThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);

}

void IntegrationPluginAlphatec::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
      qCDebug(dcAlphatec()) << "Setup" << thing << thing->params();


          uint address = thing->paramValue(alphatecWallboxPowerThingSlaveAddressParamTypeId).toUInt();
          if (address > 247 || address == 0) {
              qCWarning(dcAlphatec()) << "Setup failed, slave address is not valid" << address;
              info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
              return;
          }

          QUuid uuid = thing->paramValue(alphatecWallboxPowerThingModbusMasterUuidParamTypeId).toUuid();
          if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
              qCWarning(dcAlphatec()) << "Setup failed, hardware manager not available";
              info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
              return;
          }

          if (m_rtuConnections.contains(thing)) {
              qCDebug(dcAlphatec()) << "Already have a Alphatec connection for this thing. Cleaning up old connection and initializing new one...";
              m_rtuConnections.take(thing)->deleteLater();
          }

          AlphatecWallboxModbusRtuConnection *connection = new AlphatecWallboxModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
          connect(connection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
              if (connected) {
                  qCDebug(dcAlphatec()) << "Modbus RTU resource connected" << thing << connection->modbusRtuMaster()->serialPort();
              } else {
                  qCWarning(dcAlphatec()) << "Modbus RTU resource disconnected" << thing << connection->modbusRtuMaster()->serialPort();
              }
          });

          // Handle property changed signals
          connect(connection, &AlphatecWallboxModbusRtuConnection::evseStatusChanged, thing, [thing](quint32 evseStatus){
              if (evseStatus > 0){
                  thing->setStateValue(alphatecWallboxPowerConnectedStateTypeId,true);
                  if (evseStatus == 2){
                      thing->setStateValue(alphatecWallboxPowerPluggedInStateTypeId,true);
                      thing->setStateValue(alphatecWallboxPowerChargingStateTypeId,false);
                  }
                  if (evseStatus == 3){
                      thing->setStateValue(alphatecWallboxPowerChargingStateTypeId,true);
                      thing->setStateValue(alphatecWallboxPowerPluggedInStateTypeId,true);        //TODO löschen
                  }
              }
          });

          connect(connection, &AlphatecWallboxModbusRtuConnection::chargePowerChanged, thing, [thing](quint16 chargePower){
              chargePower = chargePower;
          });


          // FIXME: make async and check if this is really a Alphatec
          m_rtuConnections.insert(thing, connection);
          info->finish(Thing::ThingErrorNoError);



}

void IntegrationPluginAlphatec::postSetupThing(Thing *thing){

    if (thing->thingClassId() == alphatecWallboxPowerThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcAlphatec()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {

                foreach(AlphatecWallboxModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }



}

void IntegrationPluginAlphatec::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();

    qCDebug(dcAlphatec()) << "Executing action for thing" << info->thing() << info->action().actionTypeId().toString() << info->action().params();
    if (thing->thingClassId() == alphatecWallboxPowerThingClassId) {

        if (info->action().actionTypeId() == alphatecWallboxPowerMaxChargingCurrentActionTypeId){
          AlphatecWallboxModbusRtuConnection *connection = m_rtuConnections.value(thing);
            uint chargeLimit = thing->stateValue(alphatecWallboxPowerMaxChargingCurrentStateTypeId).toUInt();
            ModbusRtuReply *reply = connection -> setWriteChargeLimit(chargeLimit);
            connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, chargeLimit]{
                if (reply->error() != ModbusRtuReply::NoError) {
                    qCWarning(dcAlphatec()) << "Set charge Limit finished with error " << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                    return;
                }
                reply -> deleteLater();
            });
        }
    }
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginAlphatec::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
          hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
      }

      if (m_rtuConnections.contains(thing)) {
          m_rtuConnections.take(thing)->deleteLater();
      }

      if (myThings().isEmpty() && m_pluginTimer) {
          hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
          m_pluginTimer = nullptr;
      }

}

