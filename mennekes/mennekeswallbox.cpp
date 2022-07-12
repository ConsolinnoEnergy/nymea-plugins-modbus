#include "mennekeswallbox.h"
#include "extern-plugininfo.h"

MennekesWallbox::MennekesWallbox(MennekesModbusTcpConnection *modbusTcpConnection, quint16 slaveId, QObject *parent) :
    QObject{parent},
    m_modbusTcpConnection{modbusTcpConnection},
    m_slaveId{slaveId}
{
    connect(modbusTcpConnection, &MennekesModbusTcpConnection::currentLimitSetpointReadChanged, this, [this](quint16 currentLimitSetpoint){
        m_chargeCurrent = currentLimitSetpoint;
    });
    connect(modbusTcpConnection, &MennekesModbusTcpConnection::meterPowerL1Changed, this, [this](quint32 powerL1){
        m_powerPhase1 = powerL1;
    });
    connect(modbusTcpConnection, &MennekesModbusTcpConnection::meterPowerL2Changed, this, [this](quint32 powerL2){
        m_powerPhase2 = powerL2;
    });
    connect(modbusTcpConnection, &MennekesModbusTcpConnection::meterPowerL3Changed, this, [this](quint32 powerL3){
        m_powerPhase3 = powerL3;
    });
}

MennekesWallbox::~MennekesWallbox()
{
    qCDebug(dcMennekesWallbox()) << "Deleting SchneiderWallbox object for device with address" << m_modbusTcpConnection->hostAddress() << "and slaveId" << m_slaveId;
    delete m_modbusTcpConnection;
}


MennekesModbusTcpConnection *MennekesWallbox::modbusTcpConnection()
{
    return m_modbusTcpConnection;
}

quint16 MennekesWallbox::slaveId() const
{
    return m_slaveId;
}

void MennekesWallbox::update()
{
    if (!m_modbusTcpConnection->connected()) {
        return;
    }

    int phaseCount{0};
    double currentPower{0.0};
    if (m_powerPhase1 > 0) {
        phaseCount++;
        currentPower += m_powerPhase1;
    }
    if (m_powerPhase2 > 0) {
        phaseCount++;
        currentPower += m_powerPhase2;
    }
    if (m_powerPhase3 > 0) {
        phaseCount++;
        currentPower += m_powerPhase3;
    }
    if (phaseCount != m_phaseCount) {
        m_phaseCount = phaseCount;
        emit phaseCountChanged(phaseCount);
    }
    if (currentPower != m_currentPower) {
        m_currentPower = currentPower;
        emit currentPowerChanged(currentPower);
    }

    qCDebug(dcMennekesWallbox()) << "Charge current limit set point:" << m_chargeCurrentSetpoint;
    qCDebug(dcMennekesWallbox()) << "Received charge current limit:" << m_chargeCurrent;
    qCDebug(dcMennekesWallbox()) << "Phase count:" << m_phaseCount;
    qCDebug(dcMennekesWallbox()) << "Current power:" << m_currentPower << " (in ampere:" << (m_currentPower / 230) << ")";

    quint16 chargeCurrentSetpointSend{0};
    if (m_charging) {
        chargeCurrentSetpointSend = m_chargeCurrentSetpoint;
    }

    m_errorOccured = false;
    if (m_chargeCurrent != chargeCurrentSetpointSend) {
        qCDebug(dcMennekesWallbox()) << "Device max current:" << m_chargeCurrent <<", sending modbus command current: " << chargeCurrentSetpointSend;
        QModbusReply *reply = m_modbusTcpConnection->setCurrentLimitSetpoint(chargeCurrentSetpointSend);
        if (!reply) {
            qCWarning(dcMennekesWallbox()) << "Sending current limit failed because the reply could not be created.";
            m_errorOccured = true;
            return;
        }
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
            qCWarning(dcMennekesWallbox()) << "Modbus reply error occurred while sending max intensity socket" << error << reply->errorString();
            emit reply->finished(); // To make sure it will be deleted
        });
    }
}

bool MennekesWallbox::enableOutput(bool state)
{
    if (!m_modbusTcpConnection->connected()) {
        qCDebug(dcMennekesWallbox()) << "Can't set charging on/off, device is not connected.";
        return false;
    }

    qCDebug(dcMennekesWallbox()) << "Setting wallbox output to" << state;

    m_charging = state;
    update();
    if (m_errorOccured) {
        return false;
    }
    return true;
}

bool MennekesWallbox::setMaxAmpere(int ampereValue)
{
    if (!m_modbusTcpConnection->connected()) {
        qCDebug(dcMennekesWallbox()) << "Can't set current limit, device is not connected.";
        return false;
    }

    qCDebug(dcMennekesWallbox()) << "Setting wallbox max current to" << ampereValue;

    m_chargeCurrentSetpoint = ampereValue;
    update();
    if (m_errorOccured) {
        return false;
    }
    return true;
}
