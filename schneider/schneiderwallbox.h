#ifndef SCHNEIDERWALLBOX_H
#define SCHNEIDERWALLBOX_H

#include <QObject>

#include "schneidermodbustcpconnection.h"

class SchneiderWallbox : public QObject
{
    Q_OBJECT
public:
    explicit SchneiderWallbox(SchneiderModbusTcpConnection *modbusTcpConnection, quint16 slaveId, QObject *parent = nullptr);
    ~SchneiderWallbox();

    SchneiderModbusTcpConnection *modbusTcpConnection();

    // SchneiderModbusTcpConnection does not have a getter for this. Don't want to add it to SchneiderModbusTcpConnection, since SchneiderModbusTcpConnection is created by a script.
    quint16 slaveId() const;

    void update();

    QUuid enableOutput(bool state);
    QUuid setMaxAmpere(int ampereValue);

private:
    SchneiderModbusTcpConnection *m_modbusTcpConnection = nullptr;
    quint16 m_slaveId{1};
    bool m_charging{false};
    SchneiderModbusTcpConnection::CPWState m_cpwState{SchneiderModbusTcpConnection::CPWStateEvseNotAvailable};

    // The variable is initialized with 0 in schneidermodbustcpconnection.h, and signal is just emitted on change.
    quint16 m_recievedLifeBitRegisterValue{0};

    quint16 m_chargeCurrentSetpoint{14};   // Manual says "x>14A" to charge for Mode 3 plug on three phases.

    quint16 m_chargeCurrent{0};

signals:
    void commandExecuted(QUuid requestId, bool success);
};

#endif // SCHNEIDERWALLBOX_H
