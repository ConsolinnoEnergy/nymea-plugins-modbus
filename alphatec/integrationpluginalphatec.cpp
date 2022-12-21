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
#include <plugintimer.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

IntegrationPluginAlphatec::IntegrationPluginAlphatec()
{

}

void IntegrationPluginAlphatec::init()
{
    // Initialisation can be done here.
    qCDebug(dcAlphatec()) << "Plugin initialized.";
}

void IntegrationPluginAlphatec::setupThing(ThingSetupInfo *info)
{
    // A thing is being set up. Use info->thing() to get details of the thing, do
    // the required setup (e.g. connect to the device) and call info->finish() when done.

    qCDebug(dcAlphatec()) << "Setup thing" << info->thing();

    Thing *thing = info->thing();
    if (thing->thingClassId() == alphatecWallboxPowerThingClassId) {

        //Handle reconfigure
        if (m_alphatecDevices.contains(thing)) {
            m_alphatecDevices.take(thing)->deleteLater();
        }
        QString ip = thing->params().paramValue(alphatecWallboxPowerThingIpParamTypeId).toString();
        uint port = thing->params().paramValue(alphatecWallboxPowerThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->params().paramValue(alphatecWallboxPowerThingPortParamTypeId).toUInt();
        QHostAddress hostaddress = QHostAddress(ip);
        alphatecTcp = new AlphatecWallboxModbusTcpConnection(hostaddress,port,slaveId,this);
        qCDebug(dcAlphatec()) <<alphatecTcp;
    }
}

void IntegrationPluginAlphatec::postSetupThing(Thing *thing){

    if (!m_pluginTimer) {
        qCDebug(dcAlphatec()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this,thing] {

            updateRegisters(thing);

        });
    }

}
void IntegrationPluginAlphatec::updateRegisters(Thing *thing){

    connect(alphatecTcp, &AlphatecWallboxModbusTcpConnection::evseStatusChanged,this,[this,thing](uint evseStatus){
        if (evseStatus > 0){
            thing->setStateValue(alphatecWallboxPowerConnectedStateTypeId,true);
            if (evseStatus == 2){
                thing->setStateValue(alphatecWallboxPowerPluggedInStateTypeId,true);
                thing->setStateValue(alphatecWallboxPowerChargingStateTypeId,false);
            }
            if (evseStatus ==3){
                thing->setStateValue(alphatecWallboxPowerChargingStateTypeId,true);
                thing->setStateValue(alphatecWallboxPowerPluggedInStateTypeId,true);        //TODO löschen
            }
        }
    });
    connect(alphatecTcp,&AlphatecWallboxModbusTcpConnection::chargePowerChanged,this,[this,thing](uint chargePower){
        thing->setStateValue(alphatecWallboxPowerCurrentPowerStateTypeId,chargePower);
    });

}

void IntegrationPluginAlphatec::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    // An action is being executed. Use info->action() to get details about the action,
    // do the required operations (e.g. send a command to the network) and call info->finish() when done.

    qCDebug(dcAlphatec()) << "Executing action for thing" << info->thing() << info->action().actionTypeId().toString() << info->action().params();
    if (thing->thingClassId() == alphatecWallboxPowerThingClassId) {

        if (info->action().actionTypeId() == alphatecWallboxPowerMaxChargingCurrentActionTypeId){

            uint chargeLimit = thing->stateValue(alphatecWallboxPowerMaxChargingCurrentStateTypeId).toUInt();
            QModbusReply *reply =alphatecTcp->setWriteChargeLimit(chargeLimit);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, chargeLimit]{
                if (reply->error() != QModbusDevice::NoError) {
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
    // A thing is being removed from the system. Do the required cleanup
    // (e.g. disconnect from the device) here.


    delete m_pluginTimer;

    delete alphatecTcp;

    qCDebug(dcAlphatec()) << "Remove thing" << thing;
}

