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

#ifndef HUAWEIFUSIONSOLAR_H
#define HUAWEIFUSIONSOLAR_H

#include <QObject>
#include <QQueue>

#include "huaweifusionmodbustcpconnection.h"

class HuaweiFusionSolar : public HuaweiFusionModbusTcpConnection
{
    Q_OBJECT
public:
    explicit HuaweiFusionSolar(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent = nullptr);
    ~HuaweiFusionSolar() = default;

    virtual bool update() override;

    quint16 slaveId() const;

signals:
    void meterValuesUpdated();
    void inverterValuesUpdated();
    void battery1ValuesUpdated();
    void battery2ValuesUpdated();

private:
    quint16 m_slaveId;
    QQueue<HuaweiFusionModbusTcpConnection::Registers> m_registersQueue;
    QModbusReply *m_initReply = nullptr;

    int m_currentRegisterRequest{-1};
    void finishRequestRetryIs(bool retryRequest);

    uint m_requestRetryCounter{0};
    const uint MAX_REQUEST_RETRY_COUNT{5};

    const uint MAX_BATTERY_TIMER{10};
    bool m_battery1Available{false};
    bool m_battery2Available{false};

    // The battery is checked when the timer (=counter) reaches MAX_BATTERY_TIMER. Start with that value, so it is checked at startup.
    uint m_battery1timer{MAX_BATTERY_TIMER};
    // Start the timer for the second battery at a different value than timer one, so that the battery checks are spaced out.
    uint m_battery2timer{MAX_BATTERY_TIMER / 2}; // Set

    double m_actualInverterPower{0};

    QString exceptionToString(QModbusPdu::ExceptionCode exception);

private slots:
    void readNextRegister();
    bool valuesAreVaild(const QVector<quint16> &values, int readSize);

};

#endif // HUAWEIFUSIONSOLAR_H
