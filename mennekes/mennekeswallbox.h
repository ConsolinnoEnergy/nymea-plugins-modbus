#ifndef MENNEKESWALLBOX_H
#define MENNEKESWALLBOX_H

#include <QObject>

#include "mennekesmodbustcpconnection.h"

class MennekesWallbox : public QObject
{
    Q_OBJECT
public:
    explicit MennekesWallbox(MennekesModbusTcpConnection *modbusTcpConnection, quint16 slaveId, QObject *parent = nullptr);
    ~MennekesWallbox();

    MennekesModbusTcpConnection *modbusTcpConnection();

    // MennekesModbusTcpConnection does not have a getter for this. Don't want to add it to MennekesModbusTcpConnection, since MennekesModbusTcpConnection is created by a script.
    quint16 slaveId() const;

    void update();

    bool enableOutput(bool state);
    bool setMaxAmpere(int ampereValue);

private:
    MennekesModbusTcpConnection *m_modbusTcpConnection = nullptr;
    quint16 m_slaveId{1};
    bool m_charging{false};

    quint16 m_chargeCurrentSetpoint{0}; // Unit is Ampere
    quint16 m_chargeCurrent{0};         // Unit is Ampere
    quint32 m_powerPhase1{0};           // Unit is Watt
    quint32 m_powerPhase2{0};           // Unit is Watt
    quint32 m_powerPhase3{0};           // Unit is Watt
    quint16 m_phaseCount{0};
    double m_currentPower{0.0}; // Unit is Watt
    bool m_errorOccured{false};

signals:
    void phaseCountChanged(quint16 phaseCount);
    void currentPowerChanged(double currentPower);
};

#endif // MENNEKESWALLBOX_H
