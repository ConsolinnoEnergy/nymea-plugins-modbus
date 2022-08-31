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

#ifndef MENNEKESMODBUSTCPCONNECTION_H
#define MENNEKESMODBUSTCPCONNECTION_H

#include <QObject>

#include <modbusdatautils.h>
#include <modbustcpmaster.h>

class MennekesModbusTcpConnection : public ModbusTCPMaster
{
    Q_OBJECT
public:
    enum Registers {
        RegisterFirmware = 100,
        RegisterOcppStatus = 104,
        RegisterErrorCode1 = 105,
        RegisterErrorCode2 = 107,
        RegisterErrorCode3 = 109,
        RegisterErrorCode4 = 111,
        RegisterProtocolVersion = 120,
        RegisterVehicleState = 122,
        RegisterCpAvailable = 124,
        RegisterSafeCurrent = 131,
        RegisterCommTimeout = 132,
        RegisterMeterEnergyL1 = 200,
        RegisterMeterEnergyL2 = 202,
        RegisterMeterEnergyL3 = 204,
        RegisterMeterPowerL1 = 206,
        RegisterMeterPowerL2 = 208,
        RegisterMeterPowerL3 = 210,
        RegisterMeterCurrentL1 = 212,
        RegisterMeterCurrentL2 = 214,
        RegisterMeterCurrentL3 = 216,
        RegisterChargedEnergy = 705,
        RegisterCurrentLimitSetpointRead = 706,
        RegisterStartTimehhmmss = 707,
        RegisterChargeDuration = 709,
        RegisterEndTimehhmmss = 710,
        RegisterCurrentLimitSetpoint = 1000
    };
    Q_ENUM(Registers)

    enum OCPPstatus {
        OCPPstatusAvailable = 0,
        OCPPstatusOccupied = 1,
        OCPPstatusReserved = 2,
        OCPPstatusUnavailable = 3,
        OCPPstatusFaulted = 4,
        OCPPstatusPreparing = 5,
        OCPPstatusCharging = 6,
        OCPPstatusSuspendedEVSE = 7,
        OCPPstatusSuspendedEV = 8,
        OCPPstatusFinishing = 9
    };
    Q_ENUM(OCPPstatus)

    explicit MennekesModbusTcpConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent = nullptr);
    ~MennekesModbusTcpConnection() = default;

    /* firmware - Address: 100, Size: 2 */
    quint32 firmware() const;

    /* ocppStatus - Address: 104, Size: 1 */
    OCPPstatus ocppStatus() const;

    /* errorCode1 - Address: 105, Size: 2 */
    quint32 errorCode1() const;

    /* errorCode2 - Address: 107, Size: 2 */
    quint32 errorCode2() const;

    /* errorCode3 - Address: 109, Size: 2 */
    quint32 errorCode3() const;

    /* errorCode4 - Address: 111, Size: 2 */
    quint32 errorCode4() const;

    /* protocolVersion - Address: 120, Size: 2 */
    quint32 protocolVersion() const;

    /* vehicleState - Address: 122, Size: 1 */
    quint16 vehicleState() const;

    /* cpAvailable - Address: 124, Size: 1 */
    quint16 cpAvailable() const;
    QModbusReply *setCpAvailable(quint16 cpAvailable);

    /* safeCurrent [Ampere] - Address: 131, Size: 1 */
    quint16 safeCurrent() const;
    QModbusReply *setSafeCurrent(quint16 safeCurrent);

    /* commTimeout [Seconds] - Address: 132, Size: 1 */
    quint16 commTimeout() const;
    QModbusReply *setCommTimeout(quint16 commTimeout);

    /* meterEnergyL1 [WattHour] - Address: 200, Size: 2 */
    quint32 meterEnergyL1() const;

    /* meterEnergyL2 [WattHour] - Address: 202, Size: 2 */
    quint32 meterEnergyL2() const;

    /* meterEnergyL3 [WattHour] - Address: 204, Size: 2 */
    quint32 meterEnergyL3() const;

    /* meterPowerL1 [Watt] - Address: 206, Size: 2 */
    quint32 meterPowerL1() const;

    /* meterPowerL2 [Watt] - Address: 208, Size: 2 */
    quint32 meterPowerL2() const;

    /* meterPowerL3 [Watt] - Address: 210, Size: 2 */
    quint32 meterPowerL3() const;

    /* meterCurrentL1 [Ampere] - Address: 212, Size: 2 */
    quint32 meterCurrentL1() const;

    /* meterCurrentL2 [Ampere] - Address: 214, Size: 2 */
    quint32 meterCurrentL2() const;

    /* meterCurrentL3 [Ampere] - Address: 216, Size: 2 */
    quint32 meterCurrentL3() const;

    /* chargedEnergy [WattHour] - Address: 705, Size: 1 */
    quint16 chargedEnergy() const;

    /* currentLimitSetpointRead [Ampere] - Address: 706, Size: 1 */
    quint16 currentLimitSetpointRead() const;

    /* startTimehhmmss - Address: 707, Size: 2 */
    quint32 startTimehhmmss() const;

    /* chargeDuration [Seconds] - Address: 709, Size: 1 */
    quint16 chargeDuration() const;

    /* endTimehhmmss - Address: 710, Size: 2 */
    quint32 endTimehhmmss() const;

    /* currentLimitSetpoint [Ampere] - Address: 1000, Size: 1 */
    quint16 currentLimitSetpoint() const;
    QModbusReply *setCurrentLimitSetpoint(quint16 currentLimitSetpoint);

    virtual void initialize();
    virtual void update();

    void updateFirmware();
    void updateOcppStatus();
    void updateErrorCode1();
    void updateErrorCode2();
    void updateErrorCode3();
    void updateErrorCode4();
    void updateProtocolVersion();
    void updateVehicleState();
    void updateCpAvailable();
    void updateSafeCurrent();
    void updateCommTimeout();
    void updateMeterEnergyL1();
    void updateMeterEnergyL2();
    void updateMeterEnergyL3();
    void updateMeterPowerL1();
    void updateMeterPowerL2();
    void updateMeterPowerL3();
    void updateMeterCurrentL1();
    void updateMeterCurrentL2();
    void updateMeterCurrentL3();
    void updateChargedEnergy();
    void updateCurrentLimitSetpointRead();
    void updateStartTimehhmmss();
    void updateChargeDuration();
    void updateEndTimehhmmss();
    void updateCurrentLimitSetpoint();

signals:
    void initializationFinished();

    void firmwareChanged(quint32 firmware);
    void ocppStatusChanged(OCPPstatus ocppStatus);
    void errorCode1Changed(quint32 errorCode1);
    void errorCode2Changed(quint32 errorCode2);
    void errorCode3Changed(quint32 errorCode3);
    void errorCode4Changed(quint32 errorCode4);
    void protocolVersionChanged(quint32 protocolVersion);
    void vehicleStateChanged(quint16 vehicleState);
    void cpAvailableChanged(quint16 cpAvailable);
    void safeCurrentChanged(quint16 safeCurrent);
    void commTimeoutChanged(quint16 commTimeout);
    void meterEnergyL1Changed(quint32 meterEnergyL1);
    void meterEnergyL2Changed(quint32 meterEnergyL2);
    void meterEnergyL3Changed(quint32 meterEnergyL3);
    void meterPowerL1Changed(quint32 meterPowerL1);
    void meterPowerL2Changed(quint32 meterPowerL2);
    void meterPowerL3Changed(quint32 meterPowerL3);
    void meterCurrentL1Changed(quint32 meterCurrentL1);
    void meterCurrentL2Changed(quint32 meterCurrentL2);
    void meterCurrentL3Changed(quint32 meterCurrentL3);
    void chargedEnergyChanged(quint16 chargedEnergy);
    void currentLimitSetpointReadChanged(quint16 currentLimitSetpointRead);
    void startTimehhmmssChanged(quint32 startTimehhmmss);
    void chargeDurationChanged(quint16 chargeDuration);
    void endTimehhmmssChanged(quint32 endTimehhmmss);
    void currentLimitSetpointChanged(quint16 currentLimitSetpoint);

protected:
    QModbusReply *readFirmware();
    QModbusReply *readOcppStatus();
    QModbusReply *readErrorCode1();
    QModbusReply *readErrorCode2();
    QModbusReply *readErrorCode3();
    QModbusReply *readErrorCode4();
    QModbusReply *readProtocolVersion();
    QModbusReply *readVehicleState();
    QModbusReply *readCpAvailable();
    QModbusReply *readSafeCurrent();
    QModbusReply *readCommTimeout();
    QModbusReply *readMeterEnergyL1();
    QModbusReply *readMeterEnergyL2();
    QModbusReply *readMeterEnergyL3();
    QModbusReply *readMeterPowerL1();
    QModbusReply *readMeterPowerL2();
    QModbusReply *readMeterPowerL3();
    QModbusReply *readMeterCurrentL1();
    QModbusReply *readMeterCurrentL2();
    QModbusReply *readMeterCurrentL3();
    QModbusReply *readChargedEnergy();
    QModbusReply *readCurrentLimitSetpointRead();
    QModbusReply *readStartTimehhmmss();
    QModbusReply *readChargeDuration();
    QModbusReply *readEndTimehhmmss();
    QModbusReply *readCurrentLimitSetpoint();

    quint32 m_firmware = 0;
    OCPPstatus m_ocppStatus = OCPPstatusAvailable;
    quint32 m_errorCode1 = 0;
    quint32 m_errorCode2 = 0;
    quint32 m_errorCode3 = 0;
    quint32 m_errorCode4 = 0;
    quint32 m_protocolVersion = 0;
    quint16 m_vehicleState = 0;
    quint16 m_cpAvailable = 0;
    quint16 m_safeCurrent = 0;
    quint16 m_commTimeout = 0;
    quint32 m_meterEnergyL1 = 0;
    quint32 m_meterEnergyL2 = 0;
    quint32 m_meterEnergyL3 = 0;
    quint32 m_meterPowerL1 = 0;
    quint32 m_meterPowerL2 = 0;
    quint32 m_meterPowerL3 = 0;
    quint32 m_meterCurrentL1 = 0;
    quint32 m_meterCurrentL2 = 0;
    quint32 m_meterCurrentL3 = 0;
    quint16 m_chargedEnergy = 0;
    quint16 m_currentLimitSetpointRead = 0;
    quint32 m_startTimehhmmss = 0;
    quint16 m_chargeDuration = 0;
    quint32 m_endTimehhmmss = 0;
    quint16 m_currentLimitSetpoint = 0;

private:
    quint16 m_slaveId = 1;
    QVector<QModbusReply *> m_pendingInitReplies;

    void verifyInitFinished();


};

QDebug operator<<(QDebug debug, MennekesModbusTcpConnection *mennekesModbusTcpConnection);

#endif // MENNEKESMODBUSTCPCONNECTION_H
