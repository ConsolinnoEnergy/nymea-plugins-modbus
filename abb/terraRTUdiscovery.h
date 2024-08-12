/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef TERRARTUDISCOVERY_H
#define TERRARTUDISCOVERY_H

#include <QObject>
#include <QTimer>

#include <hardware/modbus/modbusrtuhardwareresource.h>

class TerraRTUDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit TerraRTUDiscovery(ModbusRtuHardwareResource *modbusRtuResource, QObject *parent = nullptr);
    struct Result {
        QUuid modbusRtuMasterId;
        quint16 firmwareVersion;
        quint16 slaveId;
    };

    void startDiscovery();

    QList<Result> discoveryResults() const;

signals:
    void discoveryFinished(bool modbusRtuMasterAvailable);

private slots:
    void tryConnect(ModbusRtuMaster *master, quint16 slaveId);

private:
    ModbusRtuHardwareResource *m_modbusRtuResource = nullptr;

    QList<Result> m_discoveryResults;
};

#endif // TERRARTUDISCOVERY_H
