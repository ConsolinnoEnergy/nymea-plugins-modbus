/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#ifndef A-EBERLE-PQI-DA-SMARTMODBUSRTUCONNECTION_H
#define A-EBERLE-PQI-DA-SMARTMODBUSRTUCONNECTION_H

#include <QObject>

#include <modbusdatautils.h>
#include <hardware/modbus/modbusrtumaster.h>

class a-eberle-pqi-da-smartModbusRtuConnection : public QObject
{
    Q_OBJECT
public:
    enum Registers {
        RegisterFrequency = 1008,
        RegisterVoltagePhaseA = 1010,
        RegisterVoltagePhaseB = 1012,
        RegisterVoltagePhaseC = 1014,
        RegisterCurrentPhaseA = 1586,
        RegisterCurrentPhaseB = 1588,
        RegisterCurrentPhaseC = 1590,
        RegisterPowerPhaseA = 1946,
        RegisterPowerPhaseB = 1952,
        RegisterPowerPhaseC = 1958,
        RegisterTotalCurrentPower = 1964,
        RegisterTotalEnergyProduced = 2176,
        RegisterTotalEnergyConsumed = 2184
    };
    Q_ENUM(Registers)

    explicit a-eberle-pqi-da-smartModbusRtuConnection(ModbusRtuMaster *modbusRtuMaster, quint16 slaveId, QObject *parent = nullptr);
    ~a-eberle-pqi-da-smartModbusRtuConnection() = default;

    ModbusRtuMaster *modbusRtuMaster() const;
    quint16 slaveId() const;

    bool reachable() const;

    uint checkReachableRetries() const;
    void setCheckReachableRetries(uint checkReachableRetries);

    ModbusDataUtils::ByteOrder endianness() const;
    void setEndianness(ModbusDataUtils::ByteOrder endianness);

    /* Total system power [W] - Address: 1964, Size: 2 */
    float totalCurrentPower() const;

    /* Power phase L1 [W] - Address: 1946, Size: 2 */
    float powerPhaseA() const;

    /* Power phase L2 [W] - Address: 1952, Size: 2 */
    float powerPhaseB() const;

    /* Power phase L3 [W] - Address: 1958, Size: 2 */
    float powerPhaseC() const;

    /* Total energy consumed [kWh] - Address: 2184, Size: 2 */
    float totalEnergyConsumed() const;

    /* Total energy produced [kWh] - Address: 2176, Size: 2 */
    float totalEnergyProduced() const;

    /* Frequency [Hz] - Address: 1008, Size: 2 */
    float frequency() const;

    /* Voltage phase L1 [V] - Address: 1010, Size: 2 */
    float voltagePhaseA() const;

    /* Voltage phase L2 [V] - Address: 1012, Size: 2 */
    float voltagePhaseB() const;

    /* Voltage phase L3 [V] - Address: 1014, Size: 2 */
    float voltagePhaseC() const;

    /* Current phase L1 [A] - Address: 1586, Size: 2 */
    float currentPhaseA() const;

    /* Current phase L2 [A] - Address: 1588, Size: 2 */
    float currentPhaseB() const;

    /* Current phase L3 [A] - Address: 1590, Size: 2 */
    float currentPhaseC() const;

    /* Read block from start addess 1008 with size of 8 registers containing following 4 properties:
      - Frequency [Hz] - Address: 1008, Size: 2
      - Voltage phase L1 [V] - Address: 1010, Size: 2
      - Voltage phase L2 [V] - Address: 1012, Size: 2
      - Voltage phase L3 [V] - Address: 1014, Size: 2
    */
    void updatePhaseVoltageAndFrequencyBlock();

    /* Read block from start addess 1586 with size of 6 registers containing following 3 properties:
      - Current phase L1 [A] - Address: 1586, Size: 2
      - Current phase L2 [A] - Address: 1588, Size: 2
      - Current phase L3 [A] - Address: 1590, Size: 2
    */
    void updatePhaseCurrentBlock();

    /* Read block from start addess 0 with size of 0 registers containing following 0 properties:
    */
    void updatePhasePowerBlock();

    /* Read block from start addess 0 with size of 0 registers containing following 0 properties:
    */
    void updateFrequencyAndTotalEnergyBlock();

    void updateTotalCurrentPower();
    void updatePowerPhaseA();
    void updatePowerPhaseB();
    void updatePowerPhaseC();
    void updateTotalEnergyConsumed();
    void updateTotalEnergyProduced();

    void updateFrequency();
    void updateVoltagePhaseA();
    void updateVoltagePhaseB();
    void updateVoltagePhaseC();
    void updateCurrentPhaseA();
    void updateCurrentPhaseB();
    void updateCurrentPhaseC();

    ModbusRtuReply *readTotalCurrentPower();
    ModbusRtuReply *readPowerPhaseA();
    ModbusRtuReply *readPowerPhaseB();
    ModbusRtuReply *readPowerPhaseC();
    ModbusRtuReply *readTotalEnergyConsumed();
    ModbusRtuReply *readTotalEnergyProduced();
    ModbusRtuReply *readFrequency();
    ModbusRtuReply *readVoltagePhaseA();
    ModbusRtuReply *readVoltagePhaseB();
    ModbusRtuReply *readVoltagePhaseC();
    ModbusRtuReply *readCurrentPhaseA();
    ModbusRtuReply *readCurrentPhaseB();
    ModbusRtuReply *readCurrentPhaseC();

    /* Read block from start addess 1008 with size of 8 registers containing following 4 properties:
      - Frequency [Hz] - Address: 1008, Size: 2
      - Voltage phase L1 [V] - Address: 1010, Size: 2
      - Voltage phase L2 [V] - Address: 1012, Size: 2
      - Voltage phase L3 [V] - Address: 1014, Size: 2
    */
    ModbusRtuReply *readBlockPhaseVoltageAndFrequency();

    /* Read block from start addess 1586 with size of 6 registers containing following 3 properties:
      - Current phase L1 [A] - Address: 1586, Size: 2
      - Current phase L2 [A] - Address: 1588, Size: 2
      - Current phase L3 [A] - Address: 1590, Size: 2
    */
    ModbusRtuReply *readBlockPhaseCurrent();

    /* Read block from start addess 0 with size of 0 registers containing following 0 properties:
    */
    ModbusRtuReply *readBlockPhasePower();

    /* Read block from start addess 0 with size of 0 registers containing following 0 properties:
    */
    ModbusRtuReply *readBlockFrequencyAndTotalEnergy();

    virtual bool initialize();
    virtual bool update();

signals:
    void reachableChanged(bool reachable);
    void checkReachabilityFailed();
    void checkReachableRetriesChanged(uint checkReachableRetries);

    void initializationFinished(bool success);
    void updateFinished();

    void endiannessChanged(ModbusDataUtils::ByteOrder endianness);

    void totalCurrentPowerChanged(float totalCurrentPower);
    void totalCurrentPowerReadFinished(float totalCurrentPower);
    void powerPhaseAChanged(float powerPhaseA);
    void powerPhaseAReadFinished(float powerPhaseA);
    void powerPhaseBChanged(float powerPhaseB);
    void powerPhaseBReadFinished(float powerPhaseB);
    void powerPhaseCChanged(float powerPhaseC);
    void powerPhaseCReadFinished(float powerPhaseC);
    void totalEnergyConsumedChanged(float totalEnergyConsumed);
    void totalEnergyConsumedReadFinished(float totalEnergyConsumed);
    void totalEnergyProducedChanged(float totalEnergyProduced);
    void totalEnergyProducedReadFinished(float totalEnergyProduced);
    void frequencyChanged(float frequency);
    void frequencyReadFinished(float frequency);
    void voltagePhaseAChanged(float voltagePhaseA);
    void voltagePhaseAReadFinished(float voltagePhaseA);
    void voltagePhaseBChanged(float voltagePhaseB);
    void voltagePhaseBReadFinished(float voltagePhaseB);
    void voltagePhaseCChanged(float voltagePhaseC);
    void voltagePhaseCReadFinished(float voltagePhaseC);
    void currentPhaseAChanged(float currentPhaseA);
    void currentPhaseAReadFinished(float currentPhaseA);
    void currentPhaseBChanged(float currentPhaseB);
    void currentPhaseBReadFinished(float currentPhaseB);
    void currentPhaseCChanged(float currentPhaseC);
    void currentPhaseCReadFinished(float currentPhaseC);

protected:
    float m_totalCurrentPower = 0;
    float m_powerPhaseA = 0;
    float m_powerPhaseB = 0;
    float m_powerPhaseC = 0;
    float m_totalEnergyConsumed = 0;
    float m_totalEnergyProduced = 0;
    float m_frequency = 0;
    float m_voltagePhaseA = 0;
    float m_voltagePhaseB = 0;
    float m_voltagePhaseC = 0;
    float m_currentPhaseA = 0;
    float m_currentPhaseB = 0;
    float m_currentPhaseC = 0;

    void processTotalCurrentPowerRegisterValues(const QVector<quint16> values);
    void processPowerPhaseARegisterValues(const QVector<quint16> values);
    void processPowerPhaseBRegisterValues(const QVector<quint16> values);
    void processPowerPhaseCRegisterValues(const QVector<quint16> values);
    void processTotalEnergyConsumedRegisterValues(const QVector<quint16> values);
    void processTotalEnergyProducedRegisterValues(const QVector<quint16> values);

    void processFrequencyRegisterValues(const QVector<quint16> values);
    void processVoltagePhaseARegisterValues(const QVector<quint16> values);
    void processVoltagePhaseBRegisterValues(const QVector<quint16> values);
    void processVoltagePhaseCRegisterValues(const QVector<quint16> values);

    void processCurrentPhaseARegisterValues(const QVector<quint16> values);
    void processCurrentPhaseBRegisterValues(const QVector<quint16> values);
    void processCurrentPhaseCRegisterValues(const QVector<quint16> values);




    void handleModbusError(ModbusRtuReply::Error error);
    void testReachability();

private:
    ModbusRtuMaster *m_modbusRtuMaster = nullptr;
    ModbusDataUtils::ByteOrder m_endianness = ModbusDataUtils::ByteOrderBigEndian;
    quint16 m_slaveId = 1;

    bool m_reachable = false;
    ModbusRtuReply *m_checkRechableReply = nullptr;
    uint m_checkReachableRetries = 0;
    uint m_checkReachableRetriesCount = 0;
    bool m_communicationWorking = false;
    quint8 m_communicationFailedMax = 15;
    quint8 m_communicationFailedCounter = 0;

    QVector<ModbusRtuReply *> m_pendingInitReplies;
    QVector<ModbusRtuReply *> m_pendingUpdateReplies;

    QObject *m_initObject = nullptr;
    void verifyInitFinished();
    void finishInitialization(bool success);

    void verifyUpdateFinished();

    void onReachabilityCheckFailed();
    void evaluateReachableState();

};

QDebug operator<<(QDebug debug, a-eberle-pqi-da-smartModbusRtuConnection *a-eberle-pqi-da-smartModbusRtuConnection);

#endif // A-EBERLE-PQI-DA-SMARTMODBUSRTUCONNECTION_H
