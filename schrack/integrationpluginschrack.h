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

#ifndef INTEGRATIONPLUGINSCHRACK_H
#define INTEGRATIONPLUGINSCHRACK_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "cionmodbusrtuconnection.h"

#include "extern-plugininfo.h"

#include <QObject>
#include <QTimer>

class IntegrationPluginSchrack : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginschrack.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    enum StatusBit {
        StatusBitPluggedIn = 0x0001,
        StatusBitChargeContactor1Active = 0x0002,
        StatusBitChargeContactor2Active = 0x0004,
        StatusBitVentilationRequired = 0x0008,
        StatusBitPlugLockController = 0x0010,
        StatusBitPlugLockReturn = 0x0020,
        StatusBitCollectiveDisorder = 0x0040,
        StatusBitDisorderFiLs = 0x0080,
        StatusBitCableDisorder = 0x0100,
        StatusBitCableRejected = 0x0200,
        StatusBitContactorError = 0x0400
    };
    Q_ENUM(StatusBit);
    Q_DECLARE_FLAGS(StatusBits, StatusBit)

    explicit IntegrationPluginSchrack();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    void waitForActionFinish(ThingActionInfo *info, ModbusRtuReply *reply, const StateTypeId &stateTypeId, const QVariant &value);
private:
    PluginTimer *m_refreshTimer = nullptr;

    QHash<Thing *, CionModbusRtuConnection *> m_cionConnections;
};

#endif // INTEGRATIONPLUGINSCHRACK_H
