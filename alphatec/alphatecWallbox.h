#ifndef ALPHATECWALLBOX_H
#define ALPHATECWALLBOX_H


#include <QObject>

#include "alphatecwallboxmodbustcpconnection.h"

class AlphatecWallbox : public QObject
{
    Q_OBJECT
public:
    explicit AlphatecWallbox(AlphatecWallboxModbusTcpConnection *modbusTcpConnection, QObject *parent = nullptr);

    AlphatecWallboxModbusTcpConnection *modbusTcpConnection();

    void update();

    bool enableOutput(bool state);
    bool setMaxAmpere(int ampereValue);

private:
    AlphatecWallboxModbusTcpConnection *m_modbusTcpConnection = nullptr;
    bool m_charging{false};

};

#endif // ALPHATECWALLBOX_H
