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

#ifndef INTEGRATIONPLUGINDVMODBUSIR_H
#define INTEGRATIONPLUGINDVMODBUSIR_H

#include <integrations/integrationplugin.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "dvmodbusirmodbusrtuconnection.h"

class IntegrationPluginDvModbusIR: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationplugindvmodbusir.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginDvModbusIR();

    void init() override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void thingRemoved(Thing *thing) override;

private slots:

signals:

private:
    PluginTimer *m_pluginTimer = nullptr;

    QHash<Thing *, DvModbusIRModbusRtuConnection *> m_rtuConnections;
};

#endif // INTEGRATIONPLUGINDVMODBUSIR_H
