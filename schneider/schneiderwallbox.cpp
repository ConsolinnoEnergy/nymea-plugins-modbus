#include "schneiderwallbox.h"
#include "extern-plugininfo.h"

SchneiderWallbox::SchneiderWallbox(SchneiderModbusTcpConnection *modbusTcpConnection, quint16 slaveId, QObject *parent) :
    QObject{parent},
    m_modbusTcpConnection{modbusTcpConnection},
    m_slaveId{slaveId}
{
    connect(modbusTcpConnection, &SchneiderModbusTcpConnection::remoteControllerLifeBitChanged, this, [this](quint16 remoteControllerLifeBit){
        qCDebug(dcSchneiderElectric()) << "Life bit register changed to" << remoteControllerLifeBit;
        m_recievedLifeBitRegisterValue = remoteControllerLifeBit;
    });
    connect(modbusTcpConnection, &SchneiderModbusTcpConnection::cpwStateChanged, this, [this](SchneiderModbusTcpConnection::CPWState cpwState){
        m_cpwState = cpwState;
    });
    connect(modbusTcpConnection, &SchneiderModbusTcpConnection::maxIntensitySocketChanged, this, [this](quint16 maxIntensitySocket){
        m_chargeCurrent = maxIntensitySocket;
    });

}

SchneiderWallbox::~SchneiderWallbox()
{
    qCDebug(dcSchneiderElectric()) << "Deleting SchneiderWallbox object for device with address" << m_modbusTcpConnection->hostAddress() << "and slaveId" << m_slaveId;
    delete m_modbusTcpConnection;
}


SchneiderModbusTcpConnection *SchneiderWallbox::modbusTcpConnection()
{
    return m_modbusTcpConnection;
}

quint16 SchneiderWallbox::slaveId() const
{
    return m_slaveId;
}

void SchneiderWallbox::update()
{
    if (!m_modbusTcpConnection->connected()) {
        return;
    }

    int lifeBitSend{0};
    bool sendCommand{false};
    SchneiderModbusTcpConnection::RemoteCommand remoteCommandSend{SchneiderModbusTcpConnection::RemoteCommandAcknowledgeCommand};
    quint16 chargeCurrentSetpointSend{0};
    if (m_charging) {

        if (m_recievedLifeBitRegisterValue) {
            if (m_recievedLifeBitRegisterValue == 1) {
                qCWarning(dcSchneiderElectric()) << "Life bit not reset by wallbox. Connection error or wallbox malfunction!";
            } else if (m_recievedLifeBitRegisterValue == 2) {
                qCDebug(dcSchneiderElectric()) << "Life bit register at value 2, setting it to 1 now.";
            }
        }

        // Check if station is available, set available if it is not.
        if (m_cpwState == SchneiderModbusTcpConnection::CPWStateEvseNotAvailable) {
            sendCommand = true;
            remoteCommandSend = SchneiderModbusTcpConnection::RemoteCommandSetEvcseAvailable;
        }

        // Check degraded mode

        lifeBitSend = 1;


        chargeCurrentSetpointSend = m_chargeCurrentSetpoint;
    } else {

        // Remote control is needed to turn off the charge current. Wait until the charge current is 0 before switching off remote control.
        if (m_chargeCurrent == 0) {
            if (m_recievedLifeBitRegisterValue != 2) {
                // Setting lifebit to 2 means end of remote control. Simply stopping to send 1 will result in connection timeout, and the wallbox will log that as an error.
                lifeBitSend = 2;
            }
        } else {
            // Charge current is not 0. Still need send command to end charging, so keep wallbox in remote mode.
            lifeBitSend = 1;
        }


        // Check status. Depending on status, send force stop command. Force stop command might be needed to release plug.
    }


    if (lifeBitSend) {
        QModbusReply *reply = m_modbusTcpConnection->setRemoteControllerLifeBit(lifeBitSend);
        if (!reply) {
            qCWarning(dcSchneiderElectric()) << "Sending life bit failed because the reply could not be created.";
            return;
        }
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
            qCWarning(dcSchneiderElectric()) << "Modbus reply error occurred while sending life bit" << error << reply->errorString();
            emit reply->finished(); // To make sure it will be deleted
        });
    }

    if (sendCommand) {
        QModbusReply *reply = m_modbusTcpConnection->setRemoteCommand(remoteCommandSend);
        if (!reply) {
            qCWarning(dcSchneiderElectric()) << "Sending remote command failed because the reply could not be created.";
            return;
        }
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
            qCWarning(dcSchneiderElectric()) << "Modbus reply error occurred while sending remote command" << error << reply->errorString();
            emit reply->finished(); // To make sure it will be deleted
        });
    }


    if (m_chargeCurrent != chargeCurrentSetpointSend) {
        /*
        QModbusReply *reply = m_modbusTcpConnection->setMaxIntensitySocket(currentSetpoint);
        if (!reply) {
            qCWarning(dcSchneiderElectric()) << "Sending max intensity socket failed because the reply could not be created.";
            return;
        }
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
            qCWarning(dcSchneiderElectric()) << "Modbus reply error occurred while sending max intensity socket" << error << reply->errorString();
            emit reply->finished(); // To make sure it will be deleted
        });
        */
    }
}

QUuid SchneiderWallbox::enableOutput(bool state)
{
    if (!m_modbusTcpConnection->connected()) {
        return QUuid();
    }

    m_charging = state;
    update();

    // ToDo: Actually check if the command was executed.
    QUuid returnValue{QUuid::createUuid()};
    emit commandExecuted(returnValue, true);
    return QUuid();
}

QUuid SchneiderWallbox::setMaxAmpere(int ampereValue)
{
    if (!m_modbusTcpConnection->connected()) {
        return QUuid();
    }

    m_chargeCurrentSetpoint = ampereValue;
    update();

    // ToDo: Actually check if the command was executed.
    QUuid returnValue{QUuid::createUuid()};
    emit commandExecuted(returnValue, true);
    return QUuid();
}
