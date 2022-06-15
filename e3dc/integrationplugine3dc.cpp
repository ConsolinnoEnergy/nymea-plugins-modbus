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

#include "integrationplugine3dc.h"

#include "network/networkdevicediscovery.h"
#include "hardwaremanager.h"
#include "plugininfo.h"


IntegrationPluginTemplate::IntegrationPluginTemplate()
{
    // Connection Params
    m_connectionIpParamTypeIds.insert(e3dcConnectionThingClassId, e3dcConnectionThingIpAddressParamTypeId);
    m_connectionPortParamTypeIds.insert(e3dcConnectionThingClassId, e3dcConnectionThingPortParamTypeId);
    m_connectionMacAddressParamTypeIds.insert(e3dcConnectionThingClassId, e3dcConnectionThingMacAddressParamTypeId);
    m_connectionSlaveIdParamTypeIds.insert(e3dcConnectionThingClassId, e3dcConnectionThingSlaveIdParamTypeId);

    // Connection state
    m_connectedStateTypeIds.insert(e3dcConnectionThingClassId, e3dcConnectionConnectedStateTypeId);

    // Child things
    m_connectedStateTypeIds.insert(e3dcInverterThingClassId, e3dcInverterConnectedStateTypeId);

    // Params
        //model
    m_modelIdParamTypeIds.insert(e3dcInverterThingClassId, e3dcInverterThingModelIdParamTypeId);

}

void IntegrationPluginTemplate::discoverThings(ThingDiscoveryInfo *info)
{

    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcE3dc()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){

        foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
            qCDebug(dcE3dc()) << "Found" << networkDeviceInfo;
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

            qCDebug(dcE3dc()) << networkDeviceInfo;
            ThingDescriptor descriptor( info->thingClassId() , title, description);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(m_connectionMacAddressParamTypeIds.value(info->thingClassId()), networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcE3dc()) << "This thing already exists in the system." << existingThings.first() << networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }



            ParamList params;
            params << Param(m_connectionIpParamTypeIds.value(info->thingClassId()), networkDeviceInfo.address().toString());
            params << Param(m_connectionMacAddressParamTypeIds.value(info->thingClassId()), networkDeviceInfo.macAddress());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        // Discovery done
        info->finish(Thing::ThingErrorNoError);
    });


}


void IntegrationPluginTemplate::setupThing(ThingSetupInfo *info)
{

    // Identifiy which ThingClass the Thing belongs to and start the coresponding setupDevice function
    Thing *thing = info->thing();
    qCDebug(dcE3dc()) << "Setup" << thing << "with params: " << thing->params();

    if (thing->thingClassId() == e3dcConnectionThingClassId )
    {
        setupConnection(info);

    }else if (thing->thingClassId() == e3dcInverterThingClassId) {
       Thing *parentThing = myThings().findById(thing->parentId());
       if (parentThing->setupStatus() == Thing::ThingSetupStatusComplete) {
           setupInverter(info);
       } else {
           connect(parentThing, &Thing::setupStatusChanged, info, [this, info] {
               // if for some reason the Connection was not established yet, wait until the setupStatus changed
               setupInverter(info);
           });
       }
    }else if (thing->thingClassId() == e3dcWallboxThingClassId) {
       Thing *parentThing = myThings().findById(thing->parentId());
       if (parentThing->setupStatus() == Thing::ThingSetupStatusComplete) {
           setupWallbox(info);
       } else {
           connect(parentThing, &Thing::setupStatusChanged, info, [this, info] {
               setupWallbox(info);
           });
       }
    }else if (thing->thingClassId() == e3dcBatteryThingClassId) {
       Thing *parentThing = myThings().findById(thing->parentId());
       if (parentThing->setupStatus() == Thing::ThingSetupStatusComplete) {
           setupBattery(info);
       } else {
           connect(parentThing, &Thing::setupStatusChanged, info, [this, info] {
               setupBattery(info);
           });
       }

   }else{
        Q_ASSERT_X(false, "setupThing", QString("Unhandled thingClassId: %1").arg(info->thing()->thingClassId().toString()).toUtf8());
    }

}



void IntegrationPluginTemplate::postSetupThing(Thing *thing)
{
    qCDebug(dcE3dc()) << "Post setup thing" << thing->name();
    // setup a timer
    if (!m_pluginTimer){
        qCDebug(dcE3dc()) << "Starting plugin timer";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
        // everytime the timer is reached use the onPluginTimer function to "update" the devices
        connect(m_pluginTimer, &PluginTimer::timeout, this, &IntegrationPluginTemplate::onPluginTimer);
    }

    // Check if the Connection still exists
    if (m_e3DCConnections.contains(thing->id())) {
        TCP_ModbusConnection *TCPconnection = m_e3DCConnections.value(thing->id());
        if (!TCPconnection) {
            qCWarning(dcE3dc()) << "TCP_Connection not found for" << thing;
            return;
        }
        // if thing is either an inverter, a battery or a Wallbox
        // update the devices once.
        } else if (m_e3DCInverters.contains(thing)) {
            TCP_ModbusConnection *parentConnection =  m_e3DCConnections.value(thing->parentId());
            parentConnection->updateInverter(m_e3DCInverters.value(thing) );
        } else if (m_e3DCWallboxes.contains(thing)) {
            TCP_ModbusConnection *parentConnection =  m_e3DCConnections.value(thing->parentId());
            parentConnection->updateWallbox(m_e3DCWallboxes.value(thing));
        } else if (m_e3DCBatteries.contains(thing)) {
            TCP_ModbusConnection *parentConnection =  m_e3DCConnections.value(thing->parentId());
            parentConnection->updateBattery(m_e3DCBatteries.value(thing));
        }  else {
            Q_ASSERT_X(false, "postSetupThing", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
        }



}

void IntegrationPluginTemplate::thingRemoved(Thing *thing)
{

    // we delete the Connection.
    // Since it is the Parent of the others, its also going to delete its children
    if (thing->thingClassId() == e3dcConnectionThingClassId && m_e3DCConnections.contains(thing->id())) {
        TCP_ModbusConnection *connection = m_e3DCConnections.take(thing->id());
        delete connection;
    }

    // We can also just delete one of its children
    if ((thing->thingClassId() == e3dcInverterThingClassId) && m_e3DCInverters.contains(thing)) {
        e3dcInverter *inverter = m_e3DCInverters.take(thing);
        delete inverter;
    }

    if ((thing->thingClassId() == e3dcBatteryThingClassId) && m_e3DCBatteries.contains(thing)) {
        e3dcBattery *batteries = m_e3DCBatteries.take(thing);
        delete batteries;
    }

    if ((thing->thingClassId() == e3dcWallboxThingClassId) && m_e3DCWallboxes.contains(thing)) {
        e3dcWallbox *wallbox = m_e3DCWallboxes.take(thing);
        delete wallbox;
    }




    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}


void IntegrationPluginTemplate::onPluginTimer()
{
    // Update inverter

        foreach (Thing *thing, myThings()){

            Thing *parentThing = myThings().findById(thing->parentId());
            if (!parentThing){
                qCDebug(dcE3dc()) << "Thing does not have a parent or maybe it is the parent node itself";
                continue;
            }
            // get the Tcp connection of its parent
            TCP_ModbusConnection* TcpConnection = m_e3DCConnections.value(thing->parentId());

            // update for every inverter, wallbox or battery
            qCDebug(dcE3dc()) << "Child connectionState";
            if (thing->stateValue("connected").toBool() &  m_e3DCInverters.contains(thing) ) {
                TcpConnection->updateInverter(m_e3DCInverters.value(thing));
            }

            if (thing->stateValue("connected").toBool() & m_e3DCWallboxes.contains(thing) ) {
                TcpConnection->updateWallbox(m_e3DCWallboxes.value(thing));
            }

            if (thing->stateValue("connected").toBool() & m_e3DCBatteries.contains(thing) ) {
                TcpConnection->updateBattery(m_e3DCBatteries.value(thing));
            }

        }


}

void IntegrationPluginTemplate::setupConnection(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    QHostAddress hostAddress = QHostAddress(info->thing()->paramValue(m_connectionIpParamTypeIds.value(thing->thingClassId())).toString());
    int port = info->thing()->paramValue(m_connectionPortParamTypeIds.value(thing->thingClassId())).toInt();
    int slaveId = info->thing()->paramValue(m_connectionSlaveIdParamTypeIds.value(thing->thingClassId())).toInt();


    if (m_e3DCConnections.contains(thing->id())) {
        qCDebug(dcE3dc()) << "Reconfigure TCP_connection with new address" << hostAddress;
        m_e3DCConnections.take(thing->id())->deleteLater();
    }


    // establish a new TCP_ModbusConnection
    TCP_ModbusConnection *TcpConnection = new TCP_ModbusConnection(hostAddress, port, slaveId, this);

    // Update all child things for this connection if the connectionState changed obviously only if the connectionState is true
    connect(TcpConnection, &TCP_ModbusConnection::connectionStateChanged, thing, [this, TcpConnection, thing] (bool connected) {
        qCDebug(dcE3dc()) << TcpConnection << (connected ? "connected" : "disconnected");
        // set connectionState of the parent
        thing->setStateValue(m_connectedStateTypeIds.value(thing->thingClassId()), connected);

        // Update connected state of child things
        foreach (Thing *child, myThings().filterByParentId(thing->id())) {
            // set the connected state of all children
            child->setStateValue(m_connectedStateTypeIds.value(child->thingClassId()), connected);

            // Refresh childs if connected is true
            // Do this for every inverter, wallbox or battery
            if (connected && m_e3DCInverters.contains(child) ) {
                TcpConnection->updateInverter(m_e3DCInverters.value(child));
            }

            if (connected && m_e3DCWallboxes.contains(child) ) {
                TcpConnection->updateWallbox(m_e3DCWallboxes.value(child));
            }

            if (connected && m_e3DCBatteries.contains(child) ) {
                TcpConnection->updateBattery(m_e3DCBatteries.value(child));
            }

        }
    });


    // Note: connectionStateChanged is a signal emitted by the TCPMaster class
    // This will only be casted during setup and later deleted, since the Slot is bounded to info, and this gets deleted after this
    connect(TcpConnection, &TCP_ModbusConnection::connectionStateChanged, info, [this, TcpConnection, info] (bool connected) {

        qCDebug(dcE3dc()) << "E3DC connected changed during setup:" << (connected ? "connected" : "disconnected");
        // only do anything if the connectionStateChanged from false -> true
        if (connected) {

            // so we send to 9 register the question, whether or not the Wallbox exists.
            // We count the amount of responses we get and if we have 9 we can make sure that every response came back, and we now how many are active
            // we bound the variables to the TCP_Connection so multiple Connections can be setup.
            connect(TcpConnection, &TCP_ModbusConnection::Countwallbox, info,[this, TcpConnection](bool count){
                if (count){
                    TcpConnection->setWallboxActive(TcpConnection->WallboxActive()+1);
                }else{
                    TcpConnection->setWallboxInactive(TcpConnection->WallboxInactive()+1);
                }
                qCDebug(dcE3dc()) << "We got a respond of Wallbox numero: " + (TcpConnection->WallboxActive() + TcpConnection->WallboxInactive());
                // if we got the information of all 9, we can say the discovery is finished
                if( ( TcpConnection->WallboxActive() + TcpConnection->WallboxInactive() ) == TcpConnection->MaximumAmountofWallboxes()){
                    emit TcpConnection->discoveryFinished(true);
                }

            });

            // this connection will only be triggered if startDiscovery is finished
            connect(TcpConnection, &TCP_ModbusConnection::discoveryFinished, info,[this, TcpConnection, info](bool success){
                if (success){

                    qCDebug(dcE3dc()) << "found something atleast";
                    // we can now add the Connection, since we now know that we got atleas a reply back
                    m_e3DCConnections.insert(info->thing()->id(), TcpConnection);
                    info->finish(Thing::ThingErrorNoError);
                    // now we add the children of the parent gateway Connection
                    processDiscoveryResult(info->thing(), TcpConnection);

                }else{

                    qCDebug(dcE3dc()) << " something went wrong during the Wallbox Discovery";
                }


            });
            // try to find out if wallboxes exist
            TcpConnection->startDiscovery();


        } else {
            info->finish(Thing::ThingErrorHardwareNotAvailable);
        }
    });

    // if the ThingSetup gets aborted delete the TcpConnection ...
    connect(info, &ThingSetupInfo::aborted, TcpConnection, &TCP_ModbusConnection::deleteLater);
    // ... and remove the Connection from m_e3dcConnections
    connect(TcpConnection, &TCP_ModbusConnection::destroyed, thing, [this, thing] { m_e3DCConnections.remove(thing->id()); });



}

// when the things get "autoAppeared" by the Connection we always going to Setup an Inverter and a Battery
// After the "ThingSetup" method of the Inverter Thing gets called, this setupInverter function is called to set everything up
void IntegrationPluginTemplate::setupInverter(ThingSetupInfo *info){

    Thing *thing = info->thing();
    TCP_ModbusConnection * TCPconnection = m_e3DCConnections.value(thing->parentId());

    // check if parent connection exists
    if (!TCPconnection) {
        qCWarning(dcE3dc()) << "Could not find E3dc parent connection for" << thing;
        return info->finish(Thing::ThingErrorHardwareNotAvailable);
    }
    // we create an e3dcInverter Object, where we "save" the attributes/values we want to read from the Gateway
    e3dcInverter* inverter;
    m_e3DCInverters.insert(thing, inverter);

    // everytime the Inverter gets updated, also update the thing
    connect(inverter, &e3dcInverter::currentPowerChanged, this, [thing](float currentPower){
        qCDebug(dcE3dc()) << thing << "Inverter current Power changed" << currentPower << "W";
        thing->setStateValue(e3dcInverterCurrentPowerStateTypeId, currentPower);
    });

    connect(inverter, &e3dcInverter::networkPointPowerChanged, this, [thing](float networkPointPower){
        qCDebug(dcE3dc()) << thing << "Inverter NetworkPointPower changed" << networkPointPower << "W";
        thing->setStateValue(e3dcInverterNetworkPointPowerStateTypeId, networkPointPower);
    });


    info->finish(Thing::ThingErrorNoError);
    return;


}

void IntegrationPluginTemplate::setupWallbox(ThingSetupInfo *info){

    Thing *thing = info->thing();
    TCP_ModbusConnection * TCPconnection = m_e3DCConnections.value(thing->parentId());
    // check if parent connection exists
    if (!TCPconnection) {
        qCWarning(dcE3dc()) << "Could not find E3dc parent connection for" << thing;
        return info->finish(Thing::ThingErrorHardwareNotAvailable);
    }


    e3dcWallbox* wallbox;
    m_e3DCWallboxes.insert(thing, wallbox);
    connect(wallbox, &e3dcWallbox::currentPowerChanged, this, [thing](float currentPower){
        qCDebug(dcE3dc()) << thing << "WB current Power changed" << currentPower << "W";
        thing->setStateValue(e3dcWallboxCurrentPowerStateTypeId, currentPower);
    });



    info->finish(Thing::ThingErrorNoError);
    return;


}

void IntegrationPluginTemplate::setupBattery(ThingSetupInfo *info){

    Thing *thing = info->thing();
    TCP_ModbusConnection * TCPconnection = m_e3DCConnections.value(thing->parentId());
    // check if parent connection exists
    if (!TCPconnection) {
        qCWarning(dcE3dc()) << "Could not find E3dc parent connection for" << thing;
        return info->finish(Thing::ThingErrorHardwareNotAvailable);
    }

    e3dcBattery* battery;
    m_e3DCBatteries.insert(thing, battery);

    connect(battery, &e3dcBattery::currentPowerChanged, this, [thing](float currentPower){
        qCDebug(dcE3dc()) << thing << "Battery current Power changed" << currentPower << "W";
        thing->setStateValue(e3dcBatteryCurrentPowerStateTypeId, currentPower);
    });

    connect(battery, &e3dcBattery::SOCChanged, this, [thing](float SOC){
        qCDebug(dcE3dc()) << thing << "WB SOC changed" << SOC << "%";
        thing->setStateValue(e3dcBatteryBatteryLevelStateTypeId, SOC);
    });


    info->finish(Thing::ThingErrorNoError);
    return;


}


void IntegrationPluginTemplate::processDiscoveryResult(Thing *thing, TCP_ModbusConnection *TcpConnection)
{

   qCDebug(dcE3dc()) << "Processing discovery result from" << thing->name() << TcpConnection;

   // add Wallbox(es)
   if (TcpConnection->WallboxActive() >= 1){
       // only add a Wallbox if ONE was found
       qCDebug(dcE3dc()) << "Wallbox was found and will be setup " ;
       ThingDescriptor WBdescriptor(e3dcWallboxThingClassId);
       WBdescriptor.setParentId(thing->id());
       WBdescriptor.setTitle("e3dc Wallbox");
       // Add paramTypes
       ParamList WBparams;
       WBparams.append(Param(e3dcWallboxThingModelIdParamTypeId, "e3dc_Wallbox"));
       emit autoThingsAppeared({WBdescriptor});
   }

   // We assume that there is always an Inverter // add Inverter
   qCDebug(dcE3dc()) << "Inverter will be setup " ;
   ThingDescriptor Invdescriptor(e3dcInverterThingClassId);
   Invdescriptor.setParentId(thing->id());
   Invdescriptor.setTitle("e3dc Inverter");
   // Add paramTypes
   ParamList Invparams;
   Invparams.append(Param(e3dcInverterThingModelIdParamTypeId, "e3dc_Inverter"));
   emit autoThingsAppeared({Invdescriptor});

   // We assume there is always a battery // add Battery
   qCDebug(dcE3dc()) << "Battery will be setup " ;
   ThingDescriptor Batdescriptor;
   Batdescriptor.setParentId(thing->id());
   Batdescriptor.setTitle("e3dc Battery");
   // Add paramTypes
   ParamList Batparams;
   Batparams.append(Param( e3dcBatteryThingModelIdParamTypeId, "e3dc_Battery"));
   emit autoThingsAppeared({Batdescriptor});

}
