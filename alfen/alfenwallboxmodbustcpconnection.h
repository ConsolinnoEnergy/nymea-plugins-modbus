/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2024, nymea GmbH
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* WARNING
*
* This file has been autogenerated. Any changes in this file may be overwritten.
* If you want to change something, update the register json or the tool.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef ALFENWALLBOXMODBUSTCPCONNECTION_H
#define ALFENWALLBOXMODBUSTCPCONNECTION_H

#include <QObject>

#include <modbusdatautils.h>
#include <modbustcpmaster.h>

class AlfenWallboxModbusTcpConnection : public ModbusTCPMaster
{
    Q_OBJECT
public:
    enum Registers {
        RegisterVoltagePhaseL1 = 306,
        RegisterVoltagePhaseL2 = 308,
        RegisterVoltagePhaseL3 = 310,
        RegisterCurrentPhaseL1 = 320,
        RegisterCurrentPhaseL2 = 322,
        RegisterCurrentPhaseL3 = 324,
        RegisterCurrentSum = 326,
        RegisterPowerFactorPhase1 = 328,
        RegisterPowerFactorPhase2 = 330,
        RegisterPowerFactorPhase3 = 332,
        RegisterPowerFactorSum = 334,
        RegisterFrequency = 336,
        RegisterRealPowerPhase1 = 338,
        RegisterRealPowerPhase2 = 340,
        RegisterRealPowerPhase3 = 342,
        RegisterRealPowerSum = 344,
        RegisterCurrerealEnergyDeliveredL1 = 362,
        RegisterRealEnergyDeliveredPhaseL2 = 366,
        RegisterRealEnergyDeliveredPhaseL3 = 370,
        RegisterRealEnergyDeliveredSum = 374,
        RegisterCurrerealEnergyConsumedL1 = 378,
        RegisterRealEnergyConsumedPhaseL2 = 382,
        RegisterRealEnergyConsumedPhaseL3 = 386,
        RegisterRealEnergyConsumedSum = 390,
        RegisterAvailability = 1200,
        RegisterMode3State = 1201,
        RegisterActualAppliedMaxCurrent = 1206,
        RegisterModbusSlaveMaxCurrentValidTime = 1208,
        RegisterSetpointMaxCurrent = 1210,
        RegisterActiveLoadBalancindSafeCurrent = 1212,
        RegisterSetpointMaxCurrentActive = 1214,
        RegisterPhaseUsed = 1215
    };
    Q_ENUM(Registers)

    enum Availability {
        AvailabilityInoperative = 0,
        AvailabilityOperative = 1
    };
    Q_ENUM(Availability)

    explicit AlfenWallboxModbusTcpConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent = nullptr);
    ~AlfenWallboxModbusTcpConnection() = default;

    bool reachable() const;

    ModbusDataUtils::ByteOrder endianness() const;
    void setEndianness(ModbusDataUtils::ByteOrder endianness);

    uint checkReachableRetries() const;
    void setCheckReachableRetries(uint checkReachableRetries);

    /* modbusSlaveMaxCurrent [Ampere] - Address: 1210, Size: 2 */
    float setpointMaxCurrent() const;
    QModbusReply *setSetpointMaxCurrent(float setpointMaxCurrent);

    /* activeLoadBalancindSafeCurrent [Ampere] - Address: 1212, Size: 2 */
    float activeLoadBalancindSafeCurrent() const;

    /* modbusSlaveReceivedSetpointAccountedFor - Address: 1214, Size: 1 */
    quint16 setpointMaxCurrentActive() const;

    /* chargingUsing1or3Phases - Address: 1215, Size: 1 */
    quint16 phaseUsed() const;
    QModbusReply *setPhaseUsed(quint16 phaseUsed);

    /* voltagePhaseL1 [Volt] - Address: 306, Size: 2 */
    float voltagePhaseL1() const;

    /* voltagePhaseL2 [Volt] - Address: 308, Size: 2 */
    float voltagePhaseL2() const;

    /* voltagePhaseL3 [Volt] - Address: 310, Size: 2 */
    float voltagePhaseL3() const;

    /* currentPhaseL1 [Ampere] - Address: 320, Size: 2 */
    float currentPhaseL1() const;

    /* currentPhaseL2 [Ampere] - Address: 322, Size: 2 */
    float currentPhaseL2() const;

    /* currentPhaseL3 [Ampere] - Address: 324, Size: 2 */
    float currentPhaseL3() const;

    /* currentSum [Ampere] - Address: 326, Size: 2 */
    float currentSum() const;

    /* powerFactorPhase1 [-] - Address: 328, Size: 2 */
    float powerFactorPhase1() const;

    /* powerFactorPhase2 [-] - Address: 330, Size: 2 */
    float powerFactorPhase2() const;

    /* powerFactorPhase3 [-] - Address: 332, Size: 2 */
    float powerFactorPhase3() const;

    /* powerFactorSum [-] - Address: 334, Size: 2 */
    float powerFactorSum() const;

    /* frequency [Hz] - Address: 336, Size: 2 */
    float frequency() const;

    /* realPowerPhase1 [Watt] - Address: 338, Size: 2 */
    float realPowerPhase1() const;

    /* realPowerPhase2 [Watt] - Address: 340, Size: 2 */
    float realPowerPhase2() const;

    /* realPowerPhase3 [Watt] - Address: 342, Size: 2 */
    float realPowerPhase3() const;

    /* realPowerSum [Watt] - Address: 344, Size: 2 */
    float realPowerSum() const;

    /* realEnergyDeliveredPhaseL1 [WattHour] - Address: 362, Size: 4 */
    float currerealEnergyDeliveredL1() const;

    /* realEnergyDeliveredPhaseL2 [WattHour] - Address: 366, Size: 4 */
    float realEnergyDeliveredPhaseL2() const;

    /* realEnergyDeliveredPhaseL3 [WattHour] - Address: 370, Size: 4 */
    float realEnergyDeliveredPhaseL3() const;

    /* realEnergyDeliveredSum [WattHour] - Address: 374, Size: 4 */
    float realEnergyDeliveredSum() const;

    /* realEnergyConsumedPhaseL1 [WattHour] - Address: 378, Size: 4 */
    float currerealEnergyConsumedL1() const;

    /* realEnergyConsumedPhaseL2 [WattHour] - Address: 382, Size: 4 */
    float realEnergyConsumedPhaseL2() const;

    /* realEnergyConsumedPhaseL3 [WattHour] - Address: 386, Size: 4 */
    float realEnergyConsumedPhaseL3() const;

    /* realEnergyConsumedSum [WattHour] - Address: 390, Size: 4 */
    float realEnergyConsumedSum() const;

    /* availability - Address: 1200, Size: 1 */
    Availability availability() const;

    /* mode3State61851 - Address: 1201, Size: 5 */
    QString mode3State() const;

    /* actualAppliedMaxCurrent [Ampere] - Address: 1206, Size: 2 */
    float actualAppliedMaxCurrent() const;

    /* modbusSlaveMaxCurrentValidTime [Seconds] - Address: 1208, Size: 2 */
    quint32 modbusSlaveMaxCurrentValidTime() const;

    /* Read block from start addess 306 with size of 6 registers containing following 3 properties:
      - voltagePhaseL1 [Volt] - Address: 306, Size: 2
      - voltagePhaseL2 [Volt] - Address: 308, Size: 2
      - voltagePhaseL3 [Volt] - Address: 310, Size: 2
    */
    void updateVoltageBlock();

    /* Read block from start addess 320 with size of 26 registers containing following 13 properties:
      - currentPhaseL1 [Ampere] - Address: 320, Size: 2
      - currentPhaseL2 [Ampere] - Address: 322, Size: 2
      - currentPhaseL3 [Ampere] - Address: 324, Size: 2
      - currentSum [Ampere] - Address: 326, Size: 2
      - powerFactorPhase1 [-] - Address: 328, Size: 2
      - powerFactorPhase2 [-] - Address: 330, Size: 2
      - powerFactorPhase3 [-] - Address: 332, Size: 2
      - powerFactorSum [-] - Address: 334, Size: 2
      - frequency [Hz] - Address: 336, Size: 2
      - realPowerPhase1 [Watt] - Address: 338, Size: 2
      - realPowerPhase2 [Watt] - Address: 340, Size: 2
      - realPowerPhase3 [Watt] - Address: 342, Size: 2
      - realPowerSum [Watt] - Address: 344, Size: 2
    */
    void updateCurrentPowerFactorFreqPowerBlock();

    /* Read block from start addess 362 with size of 32 registers containing following 8 properties:
      - realEnergyDeliveredPhaseL1 [WattHour] - Address: 362, Size: 4
      - realEnergyDeliveredPhaseL2 [WattHour] - Address: 366, Size: 4
      - realEnergyDeliveredPhaseL3 [WattHour] - Address: 370, Size: 4
      - realEnergyDeliveredSum [WattHour] - Address: 374, Size: 4
      - realEnergyConsumedPhaseL1 [WattHour] - Address: 378, Size: 4
      - realEnergyConsumedPhaseL2 [WattHour] - Address: 382, Size: 4
      - realEnergyConsumedPhaseL3 [WattHour] - Address: 386, Size: 4
      - realEnergyConsumedSum [WattHour] - Address: 390, Size: 4
    */
    void updateEnergyBlock();

    /* Read block from start addess 1200 with size of 10 registers containing following 4 properties:
      - availability - Address: 1200, Size: 1
      - mode3State61851 - Address: 1201, Size: 5
      - actualAppliedMaxCurrent [Ampere] - Address: 1206, Size: 2
      - modbusSlaveMaxCurrentValidTime [Seconds] - Address: 1208, Size: 2
    */
    void updateStatusAndTransactionsBlock();

    void updateSetpointMaxCurrent();
    void updateActiveLoadBalancindSafeCurrent();
    void updateSetpointMaxCurrentActive();
    void updatePhaseUsed();

    void updateVoltagePhaseL1();
    void updateVoltagePhaseL2();
    void updateVoltagePhaseL3();
    void updateCurrentPhaseL1();
    void updateCurrentPhaseL2();
    void updateCurrentPhaseL3();
    void updateCurrentSum();
    void updatePowerFactorPhase1();
    void updatePowerFactorPhase2();
    void updatePowerFactorPhase3();
    void updatePowerFactorSum();
    void updateFrequency();
    void updateRealPowerPhase1();
    void updateRealPowerPhase2();
    void updateRealPowerPhase3();
    void updateRealPowerSum();
    void updateCurrerealEnergyDeliveredL1();
    void updateRealEnergyDeliveredPhaseL2();
    void updateRealEnergyDeliveredPhaseL3();
    void updateRealEnergyDeliveredSum();
    void updateCurrerealEnergyConsumedL1();
    void updateRealEnergyConsumedPhaseL2();
    void updateRealEnergyConsumedPhaseL3();
    void updateRealEnergyConsumedSum();
    void updateAvailability();
    void updateMode3State();
    void updateActualAppliedMaxCurrent();
    void updateModbusSlaveMaxCurrentValidTime();

    QModbusReply *readSetpointMaxCurrent();
    QModbusReply *readActiveLoadBalancindSafeCurrent();
    QModbusReply *readSetpointMaxCurrentActive();
    QModbusReply *readPhaseUsed();
    QModbusReply *readVoltagePhaseL1();
    QModbusReply *readVoltagePhaseL2();
    QModbusReply *readVoltagePhaseL3();
    QModbusReply *readCurrentPhaseL1();
    QModbusReply *readCurrentPhaseL2();
    QModbusReply *readCurrentPhaseL3();
    QModbusReply *readCurrentSum();
    QModbusReply *readPowerFactorPhase1();
    QModbusReply *readPowerFactorPhase2();
    QModbusReply *readPowerFactorPhase3();
    QModbusReply *readPowerFactorSum();
    QModbusReply *readFrequency();
    QModbusReply *readRealPowerPhase1();
    QModbusReply *readRealPowerPhase2();
    QModbusReply *readRealPowerPhase3();
    QModbusReply *readRealPowerSum();
    QModbusReply *readCurrerealEnergyDeliveredL1();
    QModbusReply *readRealEnergyDeliveredPhaseL2();
    QModbusReply *readRealEnergyDeliveredPhaseL3();
    QModbusReply *readRealEnergyDeliveredSum();
    QModbusReply *readCurrerealEnergyConsumedL1();
    QModbusReply *readRealEnergyConsumedPhaseL2();
    QModbusReply *readRealEnergyConsumedPhaseL3();
    QModbusReply *readRealEnergyConsumedSum();
    QModbusReply *readAvailability();
    QModbusReply *readMode3State();
    QModbusReply *readActualAppliedMaxCurrent();
    QModbusReply *readModbusSlaveMaxCurrentValidTime();

    /* Read block from start addess 306 with size of 6 registers containing following 3 properties:
     - voltagePhaseL1 [Volt] - Address: 306, Size: 2
     - voltagePhaseL2 [Volt] - Address: 308, Size: 2
     - voltagePhaseL3 [Volt] - Address: 310, Size: 2
    */
    QModbusReply *readBlockVoltage();

    /* Read block from start addess 320 with size of 26 registers containing following 13 properties:
     - currentPhaseL1 [Ampere] - Address: 320, Size: 2
     - currentPhaseL2 [Ampere] - Address: 322, Size: 2
     - currentPhaseL3 [Ampere] - Address: 324, Size: 2
     - currentSum [Ampere] - Address: 326, Size: 2
     - powerFactorPhase1 [-] - Address: 328, Size: 2
     - powerFactorPhase2 [-] - Address: 330, Size: 2
     - powerFactorPhase3 [-] - Address: 332, Size: 2
     - powerFactorSum [-] - Address: 334, Size: 2
     - frequency [Hz] - Address: 336, Size: 2
     - realPowerPhase1 [Watt] - Address: 338, Size: 2
     - realPowerPhase2 [Watt] - Address: 340, Size: 2
     - realPowerPhase3 [Watt] - Address: 342, Size: 2
     - realPowerSum [Watt] - Address: 344, Size: 2
    */
    QModbusReply *readBlockCurrentPowerFactorFreqPower();

    /* Read block from start addess 362 with size of 32 registers containing following 8 properties:
     - realEnergyDeliveredPhaseL1 [WattHour] - Address: 362, Size: 4
     - realEnergyDeliveredPhaseL2 [WattHour] - Address: 366, Size: 4
     - realEnergyDeliveredPhaseL3 [WattHour] - Address: 370, Size: 4
     - realEnergyDeliveredSum [WattHour] - Address: 374, Size: 4
     - realEnergyConsumedPhaseL1 [WattHour] - Address: 378, Size: 4
     - realEnergyConsumedPhaseL2 [WattHour] - Address: 382, Size: 4
     - realEnergyConsumedPhaseL3 [WattHour] - Address: 386, Size: 4
     - realEnergyConsumedSum [WattHour] - Address: 390, Size: 4
    */
    QModbusReply *readBlockEnergy();

    /* Read block from start addess 1200 with size of 10 registers containing following 4 properties:
     - availability - Address: 1200, Size: 1
     - mode3State61851 - Address: 1201, Size: 5
     - actualAppliedMaxCurrent [Ampere] - Address: 1206, Size: 2
     - modbusSlaveMaxCurrentValidTime [Seconds] - Address: 1208, Size: 2
    */
    QModbusReply *readBlockStatusAndTransactions();


    virtual bool initialize();
    virtual bool update();

signals:
    void reachableChanged(bool reachable);
    void checkReachabilityFailed();
    void checkReachableRetriesChanged(uint checkReachableRetries);

    void initializationFinished(bool success);
    void updateFinished();

    void endiannessChanged(ModbusDataUtils::ByteOrder endianness);

    void setpointMaxCurrentChanged(float setpointMaxCurrent);
    void setpointMaxCurrentReadFinished(float setpointMaxCurrent);
    void activeLoadBalancindSafeCurrentChanged(float activeLoadBalancindSafeCurrent);
    void activeLoadBalancindSafeCurrentReadFinished(float activeLoadBalancindSafeCurrent);
    void setpointMaxCurrentActiveChanged(quint16 setpointMaxCurrentActive);
    void setpointMaxCurrentActiveReadFinished(quint16 setpointMaxCurrentActive);
    void phaseUsedChanged(quint16 phaseUsed);
    void phaseUsedReadFinished(quint16 phaseUsed);

    void voltagePhaseL1Changed(float voltagePhaseL1);
    void voltagePhaseL1ReadFinished(float voltagePhaseL1);
    void voltagePhaseL2Changed(float voltagePhaseL2);
    void voltagePhaseL2ReadFinished(float voltagePhaseL2);
    void voltagePhaseL3Changed(float voltagePhaseL3);
    void voltagePhaseL3ReadFinished(float voltagePhaseL3);
    void currentPhaseL1Changed(float currentPhaseL1);
    void currentPhaseL1ReadFinished(float currentPhaseL1);
    void currentPhaseL2Changed(float currentPhaseL2);
    void currentPhaseL2ReadFinished(float currentPhaseL2);
    void currentPhaseL3Changed(float currentPhaseL3);
    void currentPhaseL3ReadFinished(float currentPhaseL3);
    void currentSumChanged(float currentSum);
    void currentSumReadFinished(float currentSum);
    void powerFactorPhase1Changed(float powerFactorPhase1);
    void powerFactorPhase1ReadFinished(float powerFactorPhase1);
    void powerFactorPhase2Changed(float powerFactorPhase2);
    void powerFactorPhase2ReadFinished(float powerFactorPhase2);
    void powerFactorPhase3Changed(float powerFactorPhase3);
    void powerFactorPhase3ReadFinished(float powerFactorPhase3);
    void powerFactorSumChanged(float powerFactorSum);
    void powerFactorSumReadFinished(float powerFactorSum);
    void frequencyChanged(float frequency);
    void frequencyReadFinished(float frequency);
    void realPowerPhase1Changed(float realPowerPhase1);
    void realPowerPhase1ReadFinished(float realPowerPhase1);
    void realPowerPhase2Changed(float realPowerPhase2);
    void realPowerPhase2ReadFinished(float realPowerPhase2);
    void realPowerPhase3Changed(float realPowerPhase3);
    void realPowerPhase3ReadFinished(float realPowerPhase3);
    void realPowerSumChanged(float realPowerSum);
    void realPowerSumReadFinished(float realPowerSum);
    void currerealEnergyDeliveredL1Changed(float currerealEnergyDeliveredL1);
    void currerealEnergyDeliveredL1ReadFinished(float currerealEnergyDeliveredL1);
    void realEnergyDeliveredPhaseL2Changed(float realEnergyDeliveredPhaseL2);
    void realEnergyDeliveredPhaseL2ReadFinished(float realEnergyDeliveredPhaseL2);
    void realEnergyDeliveredPhaseL3Changed(float realEnergyDeliveredPhaseL3);
    void realEnergyDeliveredPhaseL3ReadFinished(float realEnergyDeliveredPhaseL3);
    void realEnergyDeliveredSumChanged(float realEnergyDeliveredSum);
    void realEnergyDeliveredSumReadFinished(float realEnergyDeliveredSum);
    void currerealEnergyConsumedL1Changed(float currerealEnergyConsumedL1);
    void currerealEnergyConsumedL1ReadFinished(float currerealEnergyConsumedL1);
    void realEnergyConsumedPhaseL2Changed(float realEnergyConsumedPhaseL2);
    void realEnergyConsumedPhaseL2ReadFinished(float realEnergyConsumedPhaseL2);
    void realEnergyConsumedPhaseL3Changed(float realEnergyConsumedPhaseL3);
    void realEnergyConsumedPhaseL3ReadFinished(float realEnergyConsumedPhaseL3);
    void realEnergyConsumedSumChanged(float realEnergyConsumedSum);
    void realEnergyConsumedSumReadFinished(float realEnergyConsumedSum);
    void availabilityChanged(Availability availability);
    void availabilityReadFinished(Availability availability);
    void mode3StateChanged(const QString &mode3State);
    void mode3StateReadFinished(const QString &mode3State);
    void actualAppliedMaxCurrentChanged(float actualAppliedMaxCurrent);
    void actualAppliedMaxCurrentReadFinished(float actualAppliedMaxCurrent);
    void modbusSlaveMaxCurrentValidTimeChanged(quint32 modbusSlaveMaxCurrentValidTime);
    void modbusSlaveMaxCurrentValidTimeReadFinished(quint32 modbusSlaveMaxCurrentValidTime);

protected:
    float m_setpointMaxCurrent = 0;
    float m_activeLoadBalancindSafeCurrent = 0;
    quint16 m_setpointMaxCurrentActive = 0;
    quint16 m_phaseUsed = 0;
    float m_voltagePhaseL1 = 0;
    float m_voltagePhaseL2 = 0;
    float m_voltagePhaseL3 = 0;
    float m_currentPhaseL1 = 0;
    float m_currentPhaseL2 = 0;
    float m_currentPhaseL3 = 0;
    float m_currentSum = 0;
    float m_powerFactorPhase1 = 0;
    float m_powerFactorPhase2 = 0;
    float m_powerFactorPhase3 = 0;
    float m_powerFactorSum = 0;
    float m_frequency = 0;
    float m_realPowerPhase1 = 0;
    float m_realPowerPhase2 = 0;
    float m_realPowerPhase3 = 0;
    float m_realPowerSum = 0;
    float m_currerealEnergyDeliveredL1 = 0;
    float m_realEnergyDeliveredPhaseL2 = 0;
    float m_realEnergyDeliveredPhaseL3 = 0;
    float m_realEnergyDeliveredSum = 0;
    float m_currerealEnergyConsumedL1 = 0;
    float m_realEnergyConsumedPhaseL2 = 0;
    float m_realEnergyConsumedPhaseL3 = 0;
    float m_realEnergyConsumedSum = 0;
    Availability m_availability = AvailabilityInoperative;
    QString m_mode3State;
    float m_actualAppliedMaxCurrent = 0;
    quint32 m_modbusSlaveMaxCurrentValidTime = 0;

    void processSetpointMaxCurrentRegisterValues(const QVector<quint16> values);
    void processActiveLoadBalancindSafeCurrentRegisterValues(const QVector<quint16> values);
    void processSetpointMaxCurrentActiveRegisterValues(const QVector<quint16> values);
    void processPhaseUsedRegisterValues(const QVector<quint16> values);

    void processVoltagePhaseL1RegisterValues(const QVector<quint16> values);
    void processVoltagePhaseL2RegisterValues(const QVector<quint16> values);
    void processVoltagePhaseL3RegisterValues(const QVector<quint16> values);

    void processCurrentPhaseL1RegisterValues(const QVector<quint16> values);
    void processCurrentPhaseL2RegisterValues(const QVector<quint16> values);
    void processCurrentPhaseL3RegisterValues(const QVector<quint16> values);
    void processCurrentSumRegisterValues(const QVector<quint16> values);
    void processPowerFactorPhase1RegisterValues(const QVector<quint16> values);
    void processPowerFactorPhase2RegisterValues(const QVector<quint16> values);
    void processPowerFactorPhase3RegisterValues(const QVector<quint16> values);
    void processPowerFactorSumRegisterValues(const QVector<quint16> values);
    void processFrequencyRegisterValues(const QVector<quint16> values);
    void processRealPowerPhase1RegisterValues(const QVector<quint16> values);
    void processRealPowerPhase2RegisterValues(const QVector<quint16> values);
    void processRealPowerPhase3RegisterValues(const QVector<quint16> values);
    void processRealPowerSumRegisterValues(const QVector<quint16> values);

    void processCurrerealEnergyDeliveredL1RegisterValues(const QVector<quint16> values);
    void processRealEnergyDeliveredPhaseL2RegisterValues(const QVector<quint16> values);
    void processRealEnergyDeliveredPhaseL3RegisterValues(const QVector<quint16> values);
    void processRealEnergyDeliveredSumRegisterValues(const QVector<quint16> values);
    void processCurrerealEnergyConsumedL1RegisterValues(const QVector<quint16> values);
    void processRealEnergyConsumedPhaseL2RegisterValues(const QVector<quint16> values);
    void processRealEnergyConsumedPhaseL3RegisterValues(const QVector<quint16> values);
    void processRealEnergyConsumedSumRegisterValues(const QVector<quint16> values);

    void processAvailabilityRegisterValues(const QVector<quint16> values);
    void processMode3StateRegisterValues(const QVector<quint16> values);
    void processActualAppliedMaxCurrentRegisterValues(const QVector<quint16> values);
    void processModbusSlaveMaxCurrentValidTimeRegisterValues(const QVector<quint16> values);

    void handleModbusError(QModbusDevice::Error error);
    void testReachability();

private:
    ModbusDataUtils::ByteOrder m_endianness = ModbusDataUtils::ByteOrderBigEndian;
    quint16 m_slaveId = 1;

    bool m_reachable = false;
    QModbusReply *m_checkRechableReply = nullptr;
    uint m_checkReachableRetries = 0;
    uint m_checkReachableRetriesCount = 0;
    bool m_communicationWorking = false;
    quint8 m_communicationFailedMax = 20;
    quint8 m_communicationFailedCounter = 0;

    QVector<QModbusReply *> m_pendingInitReplies;
    QVector<QModbusReply *> m_pendingUpdateReplies;

    QObject *m_initObject = nullptr;
    void verifyInitFinished();
    void finishInitialization(bool success);

    void verifyUpdateFinished();

    void onReachabilityCheckFailed();
    void evaluateReachableState();

};

QDebug operator<<(QDebug debug, AlfenWallboxModbusTcpConnection *alfenWallboxModbusTcpConnection);

#endif // ALFENWALLBOXMODBUSTCPCONNECTION_H
