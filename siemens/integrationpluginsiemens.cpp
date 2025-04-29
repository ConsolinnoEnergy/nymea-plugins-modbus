/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
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

#include "integrationpluginsiemens.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>
#include <plugintimer.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>

IntegrationPluginSiemens::IntegrationPluginSiemens()
{
}

void IntegrationPluginSiemens::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == siemensPAC2200ThingClassId) {
        info->finish(Thing::ThingErrorNoError, QT_TR_NOOP("Manually add the device using IP address and Modbus settings."));
    }
}

void IntegrationPluginSiemens::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();

    if (m_siemensDevices.contains(thing)) {
        qCDebug(dcSiemens()) << "Reconfiguring existing device" << thing->name();
        m_siemensDevices.take(thing)->deleteLater();
    } else {
        qCDebug(dcSiemens()) << "Setting up new Siemens PAC2200:" << thing->params();
    }

    QHostAddress address = QHostAddress(thing->paramValue(siemensPAC2200ThingIpParamTypeId).toString());
    uint port = thing->paramValue(siemensPAC2200ThingPortParamTypeId).toUInt();
    quint16 slaveId = thing->paramValue(siemensPAC2200ThingSlaveIdParamTypeId).toUInt();

    SiemensPAC2200ModbusTcpConnection *connection = new SiemensPAC2200ModbusTcpConnection(address, port, slaveId, this);
    SiemensPAC2200 *pac2200 = new SiemensPAC2200(connection, this);

    connect(info, &ThingSetupInfo::aborted, pac2200, &SiemensPAC2200::deleteLater);

    NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(address);
    connect(info, &ThingSetupInfo::aborted, monitor, [monitor](){ hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor); });

    // Connection handlers
    connect(connection, &SiemensPAC2200ModbusTcpConnection::reachableChanged, thing, [thing, connection](bool reachable){
        qCDebug(dcSiemens()) << "Device reachable changed to" << reachable;
        thing->setStateValue(siemensPAC2200ConnectedStateTypeId, reachable);
        if (reachable) connection->initialize();
    });

    // Initialization
    connect(connection, &SiemensPAC2200ModbusTcpConnection::initializationFinished, info, [this, thing, pac2200, monitor, info](bool success){
        if (success) {
            qCDebug(dcSiemens()) << "Device initialized successfully";
            m_siemensDevices.insert(thing, pac2200);
            m_monitors.insert(thing, monitor);
            info->finish(Thing::ThingErrorNoError);
        } else {
            qCWarning(dcSiemens()) << "Failed to initialize device";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            pac2200->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Error initializing Modbus connection"));
        }
    });

    // Data handlers
    connect(pac2200, &SiemensPAC2200::voltagePhaseAChanged, this, [thing](float voltage){
        thing->setStateValue(siemensPAC2200VoltagePhaseAStateTypeId, voltage);
    });

    connect(pac2200, &SiemensPAC2200::currentPhaseAChanged, this, [thing](float current){
        thing->setStateValue(siemensPAC2200CurrentPhaseAStateTypeId, current);
    });

    connect(pac2200, &SiemensPAC2200::totalEnergyConsumedChanged, this, [thing](float energy){
        thing->setStateValue(siemensPAC2200TotalEnergyConsumedStateTypeId, energy);
    });

    connect(pac2200, &SiemensPAC2200::currentPowerChanged, this, [thing](float power){
        thing->setStateValue(siemensPAC2200CurrentPowerStateTypeId, power);
    });

    connect(pac2200, &SiemensPAC2200::frequencyChanged, this, [thing](float frequency){
        thing->setStateValue(siemensPAC2200FrequencyStateTypeId, frequency);
    });

    connect(pac2200, &SiemensPAC2200::deviceStatusChanged, this, [this, thing](quint32 status){
        setDeviceStatus(thing, status);
    });

    // Error handling
    connect(connection, &SiemensPAC2200ModbusTcpConnection::connectionErrorOccurred, this, [this, thing](){
        handleConnectionError(thing);
    });

    // Start connection
    connection->connectDevice();
}

void IntegrationPluginSiemens::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    if (!m_pluginTimer) {
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this](){
            foreach (Thing *thing, m_siemensDevices.keys()) {
                if (m_monitors.value(thing)->reachable()) {
                    m_siemensDevices.value(thing)->update();
                }
            }
        });
    }
}

void IntegrationPluginSiemens::thingRemoved(Thing *thing)
{
    if (m_siemensDevices.contains(thing)) {
        SiemensPAC2200 *device = m_siemensDevices.take(thing);
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        device->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSiemens::executeAction(ThingActionInfo *info)
{
    // No writable actions in basic implementation
    info->finish(Thing::ThingErrorActionNotSupported);
}

void IntegrationPluginSiemens::setDeviceStatus(Thing *thing, quint32 status)
{
    QString statusMessage;
    quint8 globalState = status & 0xFF;
    
    switch (globalState) {
    case 0:
        statusMessage = "Bootloader";
        break;
    case 1:
        statusMessage = "Device Ready";
        break;
    case 2:
        statusMessage = "Device Error";
        break;
    default:
        statusMessage = "Unknown State";
    }
    thing->setStateValue(siemensPAC2200DeviceStatusStateTypeId, statusMessage);
}

void IntegrationPluginSiemens::handleConnectionError(Thing *thing)
{
    thing->setStateValue(siemensPAC2200ConnectedStateTypeId, false);
    qCWarning(dcSiemens()) << "Connection error for device" << thing->name();
}