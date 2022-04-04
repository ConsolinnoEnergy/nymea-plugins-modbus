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

#include "integrationpluginTCP_template.h"

#include "network/networkdevicediscovery.h"
#include "hardwaremanager.h"
#include "plugininfo.h"


#include "loggingcategories.h"

NYMEA_LOGGING_CATEGORY(dcTCP_Integration, "TCP_Integration")

IntegrationPluginTemplate::IntegrationPluginTemplate()
{



}

void IntegrationPluginTemplate::discoverThings(ThingDiscoveryInfo *info)
{

    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcTCP_Integration()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

            qCDebug(dcTCP_Integration()) << "Found" << networkDeviceInfo;

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

            ThingDescriptor descriptor(TCP_templateThingClassId, title, description);
            ParamList params;
            params << Param(TCP_templateThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
            params << Param(TCP_templateThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
            descriptor.setParams(params);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(TCP_templateThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcTCP_Integration()) << "This connection already exists in the system:" << networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    });


}

void IntegrationPluginTemplate::startMonitoringAutoThings()
{

}

void IntegrationPluginTemplate::setupThing(ThingSetupInfo *info)
{


    Thing *thing = info->thing();
    qCDebug(dcTCP_Integration()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == TCP_templateThingClassId) {
        QHostAddress hostAddress = QHostAddress(thing->paramValue(TCP_templateThingIpAddressParamTypeId).toString());
        if (hostAddress.isNull()) {
            qInfo() << "hostAddreess equals to Null:" + hostAddress.toString();

            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("No IP address given"));
            return;
        }

        uint port = thing->paramValue(TCP_templateThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(TCP_templateThingSlaveIdParamTypeId).toUInt();

        TCP_ModbusConnection *templateTcpConnection = new TCP_ModbusConnection(hostAddress, port, slaveId, this);
        connect(templateTcpConnection, &TCP_ModbusConnection::connectionStateChanged, this, [thing, templateTcpConnection](bool status){
            qCDebug(dcTCP_Integration()) << "Connected changed to" << status << "for" << thing;
            if (status) {
                templateTcpConnection->update();
            }

            thing->setStateValue(TCP_templateConnectedStateTypeId, status);
        });

        //alle funktionen connecten

        //connect(templateTcpConnection, &TCP_ModbusConnection::Daily_pv_powerChanged, this, [thing](float returnDaily_pv_power){
        //    qCDebug(dcTemplate()) << thing << "Daily_pv_power  changed" << returnDaily_pv_power << "°C";
        //    thing->setStateValue(templateDaily_pv_powerStateTypeId, returnDaily_pv_power);
        //});
        // is just an example doesnt really work
       //connect(templateTcpConnection, &TemplateModbusTcpConnection::exampleRegisterChanged, this, [thing](float exampleRegister ){
       //     qCDebug(dcTemplate()) << thing << "exampleRegister  changed" << exampleRegister << "RegisterUnit";
       //     thing->setStateValue(templateDaily_pv_powerStateTypeId, exampleRegister);
       // });


        m_templateTcpThings.insert(thing, templateTcpConnection);
        templateTcpConnection->connectDevice();

        info->finish(Thing::ThingErrorNoError);

    }


}

void IntegrationPluginTemplate::postSetupThing(Thing *thing)
{

 if (thing->thingClassId() == TCP_templateThingClassId){
     if (!m_pluginTimer){
         m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
         connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
             foreach (TCP_ModbusConnection *connection, m_templateTcpThings){
                 if (connection->connected()){
                     connection->update();
                 }

             }


         });
         m_pluginTimer->start();
     }

 }
}

void IntegrationPluginTemplate::thingRemoved(Thing *thing)
{


    if (thing->thingClassId() == TCP_templateThingClassId && m_templateTcpThings.contains(thing)) {
        TCP_ModbusConnection *connection = m_templateTcpThings.take(thing);
        delete connection;
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginTemplate::executeAction(ThingActionInfo *info)
{


    info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));

}


