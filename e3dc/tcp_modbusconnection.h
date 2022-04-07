/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2022, nymea GmbH
* Contact: contact@nymea.io
*
* This fileDescriptor is part of nymea.
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

#ifndef TCP_MODBUSCONNECTION_H
#define TCP_MODBUSCONNECTION_H

#include <QObject>

#include "../modbus/modbusdatautils.h"
#include "../modbus/modbustcpmaster.h"

class TCP_ModbusConnection : public ModbusTCPMaster
{
    Q_OBJECT
public:
    enum Registers {
        RegisterCurrentPower = 40068
    };
    Q_ENUM(Registers)

    explicit TCP_ModbusConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent = nullptr);
    ~TCP_ModbusConnection() = default;

    /* Inverter current Power [kW] - Address: 40068, Size: 2 */
    float currentPower() const;


    virtual void initialize();
    virtual void update();

    void updateCurrentPower();

signals:
    void initializationFinished();

    void currentPowerChanged(float currentPower);

protected:
    QModbusReply *readCurrentPower();

    float m_currentPower = 0;

private:
    quint16 m_slaveId = 1;
    QVector<QModbusReply *> m_pendingInitReplies;

    void verifyInitFinished();


};

QDebug operator<<(QDebug debug, TCP_ModbusConnection *tCP_ModbusConnection);

#endif // TCP_MODBUSCONNECTION_H
