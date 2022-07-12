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

#ifndef SCHNEIDERMODBUSTCPCONNECTION_H
#define SCHNEIDERMODBUSTCPCONNECTION_H

#include <QObject>

#include <modbusdatautils.h>
#include <modbustcpmaster.h>

class SchneiderModbusTcpConnection : public ModbusTCPMaster
{
    Q_OBJECT
public:
    enum Registers {
        RegisterCpwState = 6,
        RegisterLastChargeStatus = 9,
        RegisterRemoteCommandStatus = 20,
        RegisterErrorStatus = 23,
        RegisterChargeTime = 30,
        RegisterRemoteCommand = 150,
        RegisterMaxIntensitySocket = 301,
        RegisterMaxIntensityCluster = 310,
        RegisterStationIntensityPhaseX = 350,
        RegisterStationIntensityPhase2 = 352,
        RegisterStationIntensityPhase3 = 354,
        RegisterStationEnergy = 356,
        RegisterStationPowerTotal = 358,
        RegisterRemoteControllerLifeBit = 932,
        RegisterDegradedMode = 933,
        RegisterSessionTime = 2004
    };
    Q_ENUM(Registers)

    enum CPWState {
        CPWStateEvseNotAvailable = 0,
        CPWStateEvseAvailable = 1,
        CPWStatePlugDetected = 2,
        CPWStateEvConnected = 4,
        CPWStateEvConnected2 = 5,
        CPWStateEvConnectedVentilationRequired = 6,
        CPWStateEvseReady = 7,
        CPWStateEvReady = 8,
        CPWStateCharging = 9,
        CPWStateEvReadyVentilationRequired = 10,
        CPWStateChargingVentilationRequired = 11,
        CPWStateStopCharging = 12,
        CPWStateAlarm = 13,
        CPWStateShortcut = 14,
        CPWStateDigitalComByEvseState = 15
    };
    Q_ENUM(CPWState)

    enum EVCSEError {
        EVCSEErrorLostCommunicationWithRfidReader = 0,
        EVCSEErrorLostCommunicationWithDisplay = 1,
        EVCSEErrorCannotConnectToMasterBoard = 2,
        EVCSEErrorIncorrectPlugLockState = 3,
        EVCSEErrorIncorrectContactorState = 4,
        EVCSEErrorIncorrectSurgeArrestorState = 5,
        EVCSEErrorIncorrectAntiIntrusionState = 6,
        EVCSEErrorCannotConnectToUsDaughterBoard = 7,
        EVCSEErrorConfigurationFileMissing = 8,
        EVCSEErrorIncorrectShutterLockState = 9,
        EVCSEErrorIncorrectCircuitBreakerState = 10,
        EVCSEErrorLostCommunicationWithPowermeter = 11,
        EVCSEErrorRemoteControllerLost = 12,
        EVCSEErrorIncorrectSocketState = 13,
        EVCSEErrorIncorrectChargingPhaseNumber = 14,
        EVCSEErrorLostCommunicationWithClusterManager = 15,
        EVCSEErrorMode3CommunicationError = 16,
        EVCSEErrorIncorrectCableState = 17,
        EVCSEErrorDefaultEvChargingCableDisconnection = 18,
        EVCSEErrorShortCircuitFp1 = 19,
        EVCSEErrorOvercurrent = 20,
        EVCSEErrorNoEnergyAvailableForCharging = 21
    };
    Q_ENUM(EVCSEError)

    enum LastChargeStatus {
        LastChargeStatusCircuitBreakerEnabled = 0,
        LastChargeStatusOk = 1,
        LastChargeStatusEndedByClusterManagerLoss = 2,
        LastChargeStatusEndOfChargeInSm3 = 3,
        LastChargeStatusCommunicationError = 4,
        LastChargeStatusDisconnectionCable = 5,
        LastChargeStatusDisconnectionEv = 6,
        LastChargeStatusShortcut = 7,
        LastChargeStatusOverload = 8,
        LastChargeStatusCanceledBySupervisor = 9,
        LastChargeStatusVentilationNotAllowed = 10,
        LastChargeStatusUnexpectedContactorOpen = 11,
        LastChargeStatusSimplifiedMode3NotAllowed = 12,
        LastChargeStatusPowerSupplyInternalError = 13,
        LastChargeStatusUnexpectedPlugUnlock = 14,
        LastChargeStatusDefaultNbPhases = 15,
        LastChargeStatusDiDefaultSurgeArrestor = 69,
        LastChargeStatusDiDefaultAntiIntrusion = 70,
        LastChargeStatusDiDefaultShutterUnlock = 73,
        LastChargeStatusDiDefaultFlsi = 74,
        LastChargeStatusDiDefaultEmergencyStop = 75,
        LastChargeStatusDiDefaultUndervoltage = 76,
        LastChargeStatusDiDefaultCi = 77,
        LastChargeStatusOther = 254,
        LastChargeStatusUndefined = 255
    };
    Q_ENUM(LastChargeStatus)

    enum RemoteCommand {
        RemoteCommandAcknowledgeCommand = 0,
        RemoteCommandForceStopCharge = 3,
        RemoteCommandSuspendCharging = 4,
        RemoteCommandRestartCharging = 5,
        RemoteCommandSetEvcseUnavailable = 6,
        RemoteCommandSetEvcseAvailable = 34
    };
    Q_ENUM(RemoteCommand)

    explicit SchneiderModbusTcpConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent = nullptr);
    ~SchneiderModbusTcpConnection() = default;

    /* cpwState - Address: 6, Size: 1 */
    CPWState cpwState() const;

    /* lastChargeStatus - Address: 9, Size: 1 */
    LastChargeStatus lastChargeStatus() const;

    /* remoteCommandStatus - Address: 20, Size: 1 */
    quint16 remoteCommandStatus() const;

    /* errorStatus - Address: 23, Size: 2 */
    quint32 errorStatus() const;

    /* chargeTime - Address: 30, Size: 2 */
    quint32 chargeTime() const;

    /* remoteCommand - Address: 150, Size: 1 */
    RemoteCommand remoteCommand() const;
    QModbusReply *setRemoteCommand(RemoteCommand remoteCommand);

    /* maxIntensitySocket [Ampere] - Address: 301, Size: 1 */
    quint16 maxIntensitySocket() const;
    QModbusReply *setMaxIntensitySocket(quint16 maxIntensitySocket);

    /* maxIntensityCluster [Ampere] - Address: 310, Size: 1 */
    quint16 maxIntensityCluster() const;
    QModbusReply *setMaxIntensityCluster(quint16 maxIntensityCluster);

    /* stationIntensityPhaseX [Ampere] - Address: 350, Size: 2 */
    float stationIntensityPhaseX() const;

    /* stationIntensityPhase2 [Ampere] - Address: 352, Size: 2 */
    float stationIntensityPhase2() const;

    /* stationIntensityPhase3 [Ampere] - Address: 354, Size: 2 */
    float stationIntensityPhase3() const;

    /* stationEnergy [WattHour] - Address: 356, Size: 2 */
    quint32 stationEnergy() const;

    /* stationPowerTotal [KiloWatt] - Address: 358, Size: 2 */
    float stationPowerTotal() const;

    /* remoteControllerLifeBit - Address: 932, Size: 1 */
    quint16 remoteControllerLifeBit() const;
    QModbusReply *setRemoteControllerLifeBit(quint16 remoteControllerLifeBit);

    /* degradedMode - Address: 933, Size: 1 */
    quint16 degradedMode() const;

    /* sessionTime [Seconds] - Address: 2004, Size: 2 */
    quint32 sessionTime() const;

    virtual void initialize();
    virtual void update();

    void updateCpwState();
    void updateLastChargeStatus();
    void updateRemoteCommandStatus();
    void updateErrorStatus();
    void updateChargeTime();
    void updateRemoteCommand();
    void updateMaxIntensitySocket();
    void updateMaxIntensityCluster();
    void updateStationIntensityPhaseX();
    void updateStationIntensityPhase2();
    void updateStationIntensityPhase3();
    void updateStationEnergy();
    void updateStationPowerTotal();
    void updateRemoteControllerLifeBit();
    void updateDegradedMode();
    void updateSessionTime();

signals:
    void initializationFinished();

    void cpwStateChanged(SchneiderModbusTcpConnection::CPWState cpwState);
    void lastChargeStatusChanged(SchneiderModbusTcpConnection::LastChargeStatus lastChargeStatus);
    void remoteCommandStatusChanged(quint16 remoteCommandStatus);
    void errorStatusChanged(quint32 errorStatus);
    void chargeTimeChanged(quint32 chargeTime);
    void remoteCommandChanged(SchneiderModbusTcpConnection::RemoteCommand remoteCommand);
    void maxIntensitySocketChanged(quint16 maxIntensitySocket);
    void maxIntensityClusterChanged(quint16 maxIntensityCluster);
    void stationIntensityPhaseXChanged(float stationIntensityPhaseX);
    void stationIntensityPhase2Changed(float stationIntensityPhase2);
    void stationIntensityPhase3Changed(float stationIntensityPhase3);
    void stationEnergyChanged(quint32 stationEnergy);
    void stationPowerTotalChanged(float stationPowerTotal);
    void remoteControllerLifeBitChanged(quint16 remoteControllerLifeBit);
    void degradedModeChanged(quint16 degradedMode);
    void sessionTimeChanged(quint32 sessionTime);

protected:
    QModbusReply *readCpwState();
    QModbusReply *readLastChargeStatus();
    QModbusReply *readRemoteCommandStatus();
    QModbusReply *readErrorStatus();
    QModbusReply *readChargeTime();
    QModbusReply *readRemoteCommand();
    QModbusReply *readMaxIntensitySocket();
    QModbusReply *readMaxIntensityCluster();
    QModbusReply *readStationIntensityPhaseX();
    QModbusReply *readStationIntensityPhase2();
    QModbusReply *readStationIntensityPhase3();
    QModbusReply *readStationEnergy();
    QModbusReply *readStationPowerTotal();
    QModbusReply *readRemoteControllerLifeBit();
    QModbusReply *readDegradedMode();
    QModbusReply *readSessionTime();

    CPWState m_cpwState = CPWStateEvseNotAvailable;
    LastChargeStatus m_lastChargeStatus = LastChargeStatusOk;
    quint16 m_remoteCommandStatus = 0;
    quint32 m_errorStatus = 0;
    quint32 m_chargeTime = 0;
    RemoteCommand m_remoteCommand = RemoteCommandAcknowledgeCommand;
    quint16 m_maxIntensitySocket = 0;
    quint16 m_maxIntensityCluster = 0;
    float m_stationIntensityPhaseX = 0;
    float m_stationIntensityPhase2 = 0;
    float m_stationIntensityPhase3 = 0;
    quint32 m_stationEnergy = 0;
    float m_stationPowerTotal = 0;
    quint16 m_remoteControllerLifeBit = 0;
    quint16 m_degradedMode = 0;
    quint32 m_sessionTime = 0;

private:
    quint16 m_slaveId = 1;
    QVector<QModbusReply *> m_pendingInitReplies;

    void verifyInitFinished();


};

QDebug operator<<(QDebug debug, SchneiderModbusTcpConnection *schneiderModbusTcpConnection);

#endif // SCHNEIDERMODBUSTCPCONNECTION_H
