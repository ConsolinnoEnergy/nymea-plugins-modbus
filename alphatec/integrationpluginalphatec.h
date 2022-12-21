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

#ifndef INTEGRATIONPLUGINALPHATEC_H
#define INTEGRATIONPLUGINALPHATEC_H

#include "integrations/integrationplugin.h"
#include "alphatecwallboxmodbustcpconnection.h"
#include "alphatecWallbox.h"
#include <QObject>
#include <QHostAddress>
class PluginTimer;

class IntegrationPluginAlphatec: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginalphatec.json")
    Q_INTERFACES(IntegrationPlugin)


public:
    explicit IntegrationPluginAlphatec();

    void init() override;

    void setupThing(ThingSetupInfo *info) override;

    void postSetupThing(Thing *thing) override;

    void executeAction(ThingActionInfo *info) override;

    void thingRemoved(Thing *thing) override;

private:
    void updateRegisters(Thing *thing);
    QHash<Thing*, AlphatecWallbox*> m_alphatecDevices;
    AlphatecWallboxModbusTcpConnection *alphatecTcp = nullptr;
    PluginTimer *m_pluginTimer = nullptr;

};

#endif // INTEGRATIONPLUGINALPHATEC_H
