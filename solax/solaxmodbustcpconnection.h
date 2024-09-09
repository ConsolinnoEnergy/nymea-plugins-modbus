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

#ifndef SOLAXMODBUSTCPCONNECTION_H
#define SOLAXMODBUSTCPCONNECTION_H

#include <QObject>

#include <modbusdatautils.h>
#include <modbustcpmaster.h>

class SolaxModbusTcpConnection : public ModbusTCPMaster
{
    Q_OBJECT
public:
    enum Registers {
        RegisterUnlockPassword = 0,
        RegisterInverterCurrent = 1,
        RegisterInverterPower = 2,
        RegisterPvVoltage1 = 3,
        RegisterPvVoltage2 = 4,
        RegisterPvCurrent1 = 5,
        RegisterPvCurrent2 = 6,
        RegisterInverterFrequency = 7,
        RegisterTemperature = 8,
        RegisterRunMode = 9,
        RegisterPowerDc1 = 10,
        RegisterPowerDc2 = 11,
        RegisterModuleName = 14,
        RegisterBatVoltageCharge1 = 20,
        RegisterBatCurrentCharge1 = 21,
        RegisterBatPowerCharge1 = 22,
        RegisterBmsConnectState = 23,
        RegisterTemperatureBat = 24,
        RegisterBatteryCapacity = 28,
        RegisterInverterFaultBits = 64,
        RegisterWriteExportLimit = 66,
        RegisterBmsWarningLsb = 68,
        RegisterBmsWarningMsb = 69,
        RegisterFeedinPower = 70,
        RegisterFeedinEnergyTotal = 72,
        RegisterConsumEnergyTotal = 74,
        RegisterGridVoltageR = 106,
        RegisterGridCurrentR = 107,
        RegisterGridPowerR = 108,
        RegisterGridFrequencyR = 109,
        RegisterGridVoltageS = 110,
        RegisterGridCurrentS = 111,
        RegisterGridPowerS = 112,
        RegisterGridFrequencyS = 113,
        RegisterGridVoltageT = 114,
        RegisterGridCurrentT = 115,
        RegisterGridPowerT = 116,
        RegisterGridFrequencyT = 117,
        RegisterFirmwareVersion = 125,
        RegisterSolarEnergyTotal = 148,
        RegisterSolarEnergyToday = 150,
        RegisterModeType = 160,
        RegisterPvPowerLimit = 162,
        RegisterForceBatteryPower = 164,
        RegisterBatteryTimeout = 166,
        RegisterReadExportLimit = 182,
        RegisterMeter1CommunicationState = 184,
        RegisterInverterType = 186
    };
    Q_ENUM(Registers)

    enum RunMode {
        RunModeWaitMode = 0,
        RunModeCheckMode = 1,
        RunModeNormalMode = 2,
        RunModeFaultMode = 3,
        RunModePermanentFaultMode = 4,
        RunModeUpdateMode = 5,
        RunModeEpsCheckMode = 6,
        RunModeEpsMode = 7,
        RunModeSelfTest = 8,
        RunModeIdleMode = 9
    };
    Q_ENUM(RunMode)

    explicit SolaxModbusTcpConnection(const QHostAddress &hostAddress, uint port, quint16 slaveId, QObject *parent = nullptr);
    ~SolaxModbusTcpConnection() = default;

    bool reachable() const;

    ModbusDataUtils::ByteOrder endianness() const;
    void setEndianness(ModbusDataUtils::ByteOrder endianness);

    uint checkReachableRetries() const;
    void setCheckReachableRetries(uint checkReachableRetries);

    /* Unlock password (0x00) - Address: 0, Size: 1 */
    QModbusReply *setUnlockPassword(quint16 unlockPassword);

    /* Battery state of charge (0x1C) [%] - Address: 28, Size: 1 */
    quint16 batteryCapacity() const;

    /* BMS warning bits lsb (0x44) - Address: 68, Size: 1 */
    quint16 bmsWarningLsb() const;

    /* BMS warning bits msb (0x45) - Address: 69, Size: 1 */
    quint16 bmsWarningMsb() const;

    /* Inverter fault bits (0x40) - Address: 64, Size: 2 */
    quint32 inverterFaultBits() const;

    /* Meter 1 communication status (0xB8) - Address: 184, Size: 1 */
    quint16 meter1CommunicationState() const;

    /* Read grid export limit (0xB6) [W] - Address: 182, Size: 1 */
    float readExportLimit() const;

    /* Write grid export limit (0x42) [W] - Address: 66, Size: 1 */
    QModbusReply *setWriteExportLimit(float writeExportLimit);

    /* Firmware version (0x7D) - Address: 125, Size: 1 */
    quint16 firmwareVersion() const;

    /* Inverter rated power (0xBA) [W] - Address: 186, Size: 1 */
    quint16 inverterType() const;

    /* Factory name (0x07) - Address: 7, Size: 7 */
    QString factoryName() const;

    /* Module name (0x0E) - Address: 14, Size: 7 */
    QString moduleName() const;

    /* Inverter voltage (0x00) [V] - Address: 0, Size: 1 */
    float inverterVoltage() const;

    /* Inverter current (0x01) [A] - Address: 1, Size: 1 */
    float inverterCurrent() const;

    /* Inverter power (0x02) [W] - Address: 2, Size: 1 */
    qint16 inverterPower() const;

    /* PV voltage 1 (0x03) [V] - Address: 3, Size: 1 */
    float pvVoltage1() const;

    /* PV voltage 2 (0x04) [V] - Address: 4, Size: 1 */
    float pvVoltage2() const;

    /* PV current 1 (0x05) [A] - Address: 5, Size: 1 */
    float pvCurrent1() const;

    /* PV current 2 (0x06) [A] - Address: 6, Size: 1 */
    float pvCurrent2() const;

    /* Inverter frequenxy (0x07) [Hz] - Address: 7, Size: 1 */
    float inverterFrequency() const;

    /* Radiator temperature (0x08) [°C] - Address: 8, Size: 1 */
    qint16 temperature() const;

    /* Run mode (0x09) - Address: 9, Size: 1 */
    RunMode runMode() const;

    /* Power DC 1 (0x0A) [W] - Address: 10, Size: 1 */
    quint16 powerDc1() const;

    /* Power DC 2 (0x0B) [W] - Address: 11, Size: 1 */
    quint16 powerDc2() const;

    /* Battery voltage charge 1 (0x14) [V] - Address: 20, Size: 1 */
    float batVoltageCharge1() const;

    /* Battery current charge 1 (0x15) [A] - Address: 21, Size: 1 */
    float batCurrentCharge1() const;

    /* Battery power charge1 (0x16) [W] - Address: 22, Size: 1 */
    qint16 batPowerCharge1() const;

    /* BMS connected state (0x17) - Address: 23, Size: 1 */
    quint16 bmsConnectState() const;

    /* Battery temperature [°C] - Address: 24, Size: 1 */
    qint16 temperatureBat() const;

    /* Power to grid (0x46). Positive means exported power, negative means consumed power. [W] - Address: 70, Size: 2 */
    qint32 feedinPower() const;

    /* Exported energy, total (0x48) [kWh] - Address: 72, Size: 2 */
    float feedinEnergyTotal() const;

    /* Consumed energy, total (0x4A) [kWh] - Address: 74, Size: 2 */
    float consumEnergyTotal() const;

    /* Phase R voltage (0x6A) [V] - Address: 106, Size: 1 */
    float gridVoltageR() const;

    /* Phase R current (0x6B) [A] - Address: 107, Size: 1 */
    float gridCurrentR() const;

    /* Phase R power (0x6C) [W] - Address: 108, Size: 1 */
    qint16 gridPowerR() const;

    /* Phase R frequency (0x6D) [Hz] - Address: 109, Size: 1 */
    float gridFrequencyR() const;

    /* Phase S voltage (0x6E) [V] - Address: 110, Size: 1 */
    float gridVoltageS() const;

    /* Phase S current (0x6F) [A] - Address: 111, Size: 1 */
    float gridCurrentS() const;

    /* Phase S power (0x70) [W] - Address: 112, Size: 1 */
    qint16 gridPowerS() const;

    /* Phase S frequency (0x71) [Hz] - Address: 113, Size: 1 */
    float gridFrequencyS() const;

    /* Phase T voltage (0x72) [V] - Address: 114, Size: 1 */
    float gridVoltageT() const;

    /* Phase T current (0x73) [A] - Address: 115, Size: 1 */
    float gridCurrentT() const;

    /* Phase T power (0x74) [W] - Address: 116, Size: 1 */
    qint16 gridPowerT() const;

    /* Phase T frequency (0x75) [Hz] - Address: 117, Size: 1 */
    float gridFrequencyT() const;

    /* Solar energy produced total (0x94) [kWh] - Address: 148, Size: 2 */
    float solarEnergyTotal() const;

    /* Solar energy produced today (0x96) [kWh] - Address: 150, Size: 1 */
    float solarEnergyToday() const;

    /* Control mode and target type (0xA0) - Address: 160, Size: 2 */
    QModbusReply *setModeType(quint32 modeType);

    /* PV power limit (0xA2) [W] - Address: 162, Size: 2 */
    QModbusReply *setPvPowerLimit(quint32 pvPowerLimit);

    /* Battery power (0xA4) [W] - Address: 164, Size: 2 */
    QModbusReply *setForceBatteryPower(quint32 forceBatteryPower);

    /* Timeout for battery (0xA6) - Address: 166, Size: 2 */
    QModbusReply *setBatteryTimeout(quint32 batteryTimeout);

    /* Read block from start addess 7 with size of 14 registers containing following 2 properties:
      - Factory name (0x07) - Address: 7, Size: 7
      - Module name (0x0E) - Address: 14, Size: 7
    */
    void updateIdentificationBlock();

    /* Read block from start addess 0 with size of 12 registers containing following 12 properties:
      - Inverter voltage (0x00) [V] - Address: 0, Size: 1
      - Inverter current (0x01) [A] - Address: 1, Size: 1
      - Inverter power (0x02) [W] - Address: 2, Size: 1
      - PV voltage 1 (0x03) [V] - Address: 3, Size: 1
      - PV voltage 2 (0x04) [V] - Address: 4, Size: 1
      - PV current 1 (0x05) [A] - Address: 5, Size: 1
      - PV current 2 (0x06) [A] - Address: 6, Size: 1
      - Inverter frequenxy (0x07) [Hz] - Address: 7, Size: 1
      - Radiator temperature (0x08) [°C] - Address: 8, Size: 1
      - Run mode (0x09) - Address: 9, Size: 1
      - Power DC 1 (0x0A) [W] - Address: 10, Size: 1
      - Power DC 2 (0x0B) [W] - Address: 11, Size: 1
    */
    void updatePvVoltageAndCurrentBlock();

    /* Read block from start addess 20 with size of 5 registers containing following 5 properties:
      - Battery voltage charge 1 (0x14) [V] - Address: 20, Size: 1
      - Battery current charge 1 (0x15) [A] - Address: 21, Size: 1
      - Battery power charge1 (0x16) [W] - Address: 22, Size: 1
      - BMS connected state (0x17) - Address: 23, Size: 1
      - Battery temperature [°C] - Address: 24, Size: 1
    */
    void updateBatPowerAndStateBlock();

    /* Read block from start addess 70 with size of 6 registers containing following 3 properties:
      - Power to grid (0x46). Positive means exported power, negative means consumed power. [W] - Address: 70, Size: 2
      - Exported energy, total (0x48) [kWh] - Address: 72, Size: 2
      - Consumed energy, total (0x4A) [kWh] - Address: 74, Size: 2
    */
    void updateMeterDataBlock();

    /* Read block from start addess 106 with size of 12 registers containing following 12 properties:
      - Phase R voltage (0x6A) [V] - Address: 106, Size: 1
      - Phase R current (0x6B) [A] - Address: 107, Size: 1
      - Phase R power (0x6C) [W] - Address: 108, Size: 1
      - Phase R frequency (0x6D) [Hz] - Address: 109, Size: 1
      - Phase S voltage (0x6E) [V] - Address: 110, Size: 1
      - Phase S current (0x6F) [A] - Address: 111, Size: 1
      - Phase S power (0x70) [W] - Address: 112, Size: 1
      - Phase S frequency (0x71) [Hz] - Address: 113, Size: 1
      - Phase T voltage (0x72) [V] - Address: 114, Size: 1
      - Phase T current (0x73) [A] - Address: 115, Size: 1
      - Phase T power (0x74) [W] - Address: 116, Size: 1
      - Phase T frequency (0x75) [Hz] - Address: 117, Size: 1
    */
    void updatePhasesDataBlock();

    /* Read block from start addess 148 with size of 3 registers containing following 2 properties:
      - Solar energy produced total (0x94) [kWh] - Address: 148, Size: 2
      - Solar energy produced today (0x96) [kWh] - Address: 150, Size: 1
    */
    void updateSolarEnergyBlock();

    void updateBatteryCapacity();
    void updateBmsWarningLsb();
    void updateBmsWarningMsb();
    void updateInverterFaultBits();
    void updateMeter1CommunicationState();
    void updateReadExportLimit();

    void updateFactoryName();
    void updateModuleName();
    void updateInverterVoltage();
    void updateInverterCurrent();
    void updateInverterPower();
    void updatePvVoltage1();
    void updatePvVoltage2();
    void updatePvCurrent1();
    void updatePvCurrent2();
    void updateInverterFrequency();
    void updateTemperature();
    void updateRunMode();
    void updatePowerDc1();
    void updatePowerDc2();
    void updateBatVoltageCharge1();
    void updateBatCurrentCharge1();
    void updateBatPowerCharge1();
    void updateBmsConnectState();
    void updateTemperatureBat();
    void updateFeedinPower();
    void updateFeedinEnergyTotal();
    void updateConsumEnergyTotal();
    void updateGridVoltageR();
    void updateGridCurrentR();
    void updateGridPowerR();
    void updateGridFrequencyR();
    void updateGridVoltageS();
    void updateGridCurrentS();
    void updateGridPowerS();
    void updateGridFrequencyS();
    void updateGridVoltageT();
    void updateGridCurrentT();
    void updateGridPowerT();
    void updateGridFrequencyT();
    void updateSolarEnergyTotal();
    void updateSolarEnergyToday();

    QModbusReply *readBatteryCapacity();
    QModbusReply *readBmsWarningLsb();
    QModbusReply *readBmsWarningMsb();
    QModbusReply *readInverterFaultBits();
    QModbusReply *readMeter1CommunicationState();
    QModbusReply *readReadExportLimit();
    QModbusReply *readFirmwareVersion();
    QModbusReply *readInverterType();
    QModbusReply *readFactoryName();
    QModbusReply *readModuleName();
    QModbusReply *readInverterVoltage();
    QModbusReply *readInverterCurrent();
    QModbusReply *readInverterPower();
    QModbusReply *readPvVoltage1();
    QModbusReply *readPvVoltage2();
    QModbusReply *readPvCurrent1();
    QModbusReply *readPvCurrent2();
    QModbusReply *readInverterFrequency();
    QModbusReply *readTemperature();
    QModbusReply *readRunMode();
    QModbusReply *readPowerDc1();
    QModbusReply *readPowerDc2();
    QModbusReply *readBatVoltageCharge1();
    QModbusReply *readBatCurrentCharge1();
    QModbusReply *readBatPowerCharge1();
    QModbusReply *readBmsConnectState();
    QModbusReply *readTemperatureBat();
    QModbusReply *readFeedinPower();
    QModbusReply *readFeedinEnergyTotal();
    QModbusReply *readConsumEnergyTotal();
    QModbusReply *readGridVoltageR();
    QModbusReply *readGridCurrentR();
    QModbusReply *readGridPowerR();
    QModbusReply *readGridFrequencyR();
    QModbusReply *readGridVoltageS();
    QModbusReply *readGridCurrentS();
    QModbusReply *readGridPowerS();
    QModbusReply *readGridFrequencyS();
    QModbusReply *readGridVoltageT();
    QModbusReply *readGridCurrentT();
    QModbusReply *readGridPowerT();
    QModbusReply *readGridFrequencyT();
    QModbusReply *readSolarEnergyTotal();
    QModbusReply *readSolarEnergyToday();

    /* Read block from start addess 7 with size of 14 registers containing following 2 properties:
     - Factory name (0x07) - Address: 7, Size: 7
     - Module name (0x0E) - Address: 14, Size: 7
    */
    QModbusReply *readBlockIdentification();

    /* Read block from start addess 0 with size of 12 registers containing following 12 properties:
     - Inverter voltage (0x00) [V] - Address: 0, Size: 1
     - Inverter current (0x01) [A] - Address: 1, Size: 1
     - Inverter power (0x02) [W] - Address: 2, Size: 1
     - PV voltage 1 (0x03) [V] - Address: 3, Size: 1
     - PV voltage 2 (0x04) [V] - Address: 4, Size: 1
     - PV current 1 (0x05) [A] - Address: 5, Size: 1
     - PV current 2 (0x06) [A] - Address: 6, Size: 1
     - Inverter frequenxy (0x07) [Hz] - Address: 7, Size: 1
     - Radiator temperature (0x08) [°C] - Address: 8, Size: 1
     - Run mode (0x09) - Address: 9, Size: 1
     - Power DC 1 (0x0A) [W] - Address: 10, Size: 1
     - Power DC 2 (0x0B) [W] - Address: 11, Size: 1
    */
    QModbusReply *readBlockPvVoltageAndCurrent();

    /* Read block from start addess 20 with size of 5 registers containing following 5 properties:
     - Battery voltage charge 1 (0x14) [V] - Address: 20, Size: 1
     - Battery current charge 1 (0x15) [A] - Address: 21, Size: 1
     - Battery power charge1 (0x16) [W] - Address: 22, Size: 1
     - BMS connected state (0x17) - Address: 23, Size: 1
     - Battery temperature [°C] - Address: 24, Size: 1
    */
    QModbusReply *readBlockBatPowerAndState();

    /* Read block from start addess 70 with size of 6 registers containing following 3 properties:
     - Power to grid (0x46). Positive means exported power, negative means consumed power. [W] - Address: 70, Size: 2
     - Exported energy, total (0x48) [kWh] - Address: 72, Size: 2
     - Consumed energy, total (0x4A) [kWh] - Address: 74, Size: 2
    */
    QModbusReply *readBlockMeterData();

    /* Read block from start addess 106 with size of 12 registers containing following 12 properties:
     - Phase R voltage (0x6A) [V] - Address: 106, Size: 1
     - Phase R current (0x6B) [A] - Address: 107, Size: 1
     - Phase R power (0x6C) [W] - Address: 108, Size: 1
     - Phase R frequency (0x6D) [Hz] - Address: 109, Size: 1
     - Phase S voltage (0x6E) [V] - Address: 110, Size: 1
     - Phase S current (0x6F) [A] - Address: 111, Size: 1
     - Phase S power (0x70) [W] - Address: 112, Size: 1
     - Phase S frequency (0x71) [Hz] - Address: 113, Size: 1
     - Phase T voltage (0x72) [V] - Address: 114, Size: 1
     - Phase T current (0x73) [A] - Address: 115, Size: 1
     - Phase T power (0x74) [W] - Address: 116, Size: 1
     - Phase T frequency (0x75) [Hz] - Address: 117, Size: 1
    */
    QModbusReply *readBlockPhasesData();

    /* Read block from start addess 148 with size of 3 registers containing following 2 properties:
     - Solar energy produced total (0x94) [kWh] - Address: 148, Size: 2
     - Solar energy produced today (0x96) [kWh] - Address: 150, Size: 1
    */
    QModbusReply *readBlockSolarEnergy();


    virtual bool initialize();
    virtual void initialize2();
    virtual void initialize3();
    virtual bool update();
    virtual void update2();
    virtual void update3();
    virtual void update4();
    virtual void update5();
    virtual void update6();
    virtual void update7();
    virtual void update8();
    virtual void update9();
    virtual void update10();
    virtual void update11();

signals:
    void reachableChanged(bool reachable);
    void checkReachabilityFailed();
    void checkReachableRetriesChanged(uint checkReachableRetries);

    void initializationFinished(bool success);
    void updateFinished();

    void endiannessChanged(ModbusDataUtils::ByteOrder endianness);

    void batteryCapacityChanged(quint16 batteryCapacity);
    void batteryCapacityReadFinished(quint16 batteryCapacity);
    void bmsWarningLsbChanged(quint16 bmsWarningLsb);
    void bmsWarningLsbReadFinished(quint16 bmsWarningLsb);
    void bmsWarningMsbChanged(quint16 bmsWarningMsb);
    void bmsWarningMsbReadFinished(quint16 bmsWarningMsb);
    void inverterFaultBitsChanged(quint32 inverterFaultBits);
    void inverterFaultBitsReadFinished(quint32 inverterFaultBits);
    void meter1CommunicationStateChanged(quint16 meter1CommunicationState);
    void meter1CommunicationStateReadFinished(quint16 meter1CommunicationState);
    void readExportLimitChanged(float readExportLimit);
    void readExportLimitReadFinished(float readExportLimit);
    void firmwareVersionChanged(quint16 firmwareVersion);
    void firmwareVersionReadFinished(quint16 firmwareVersion);
    void inverterTypeChanged(quint16 inverterType);
    void inverterTypeReadFinished(quint16 inverterType);

    void factoryNameChanged(const QString &factoryName);
    void factoryNameReadFinished(const QString &factoryName);
    void moduleNameChanged(const QString &moduleName);
    void moduleNameReadFinished(const QString &moduleName);
    void inverterVoltageChanged(float inverterVoltage);
    void inverterVoltageReadFinished(float inverterVoltage);
    void inverterCurrentChanged(float inverterCurrent);
    void inverterCurrentReadFinished(float inverterCurrent);
    void inverterPowerChanged(qint16 inverterPower);
    void inverterPowerReadFinished(qint16 inverterPower);
    void pvVoltage1Changed(float pvVoltage1);
    void pvVoltage1ReadFinished(float pvVoltage1);
    void pvVoltage2Changed(float pvVoltage2);
    void pvVoltage2ReadFinished(float pvVoltage2);
    void pvCurrent1Changed(float pvCurrent1);
    void pvCurrent1ReadFinished(float pvCurrent1);
    void pvCurrent2Changed(float pvCurrent2);
    void pvCurrent2ReadFinished(float pvCurrent2);
    void inverterFrequencyChanged(float inverterFrequency);
    void inverterFrequencyReadFinished(float inverterFrequency);
    void temperatureChanged(qint16 temperature);
    void temperatureReadFinished(qint16 temperature);
    void runModeChanged(RunMode runMode);
    void runModeReadFinished(RunMode runMode);
    void powerDc1Changed(quint16 powerDc1);
    void powerDc1ReadFinished(quint16 powerDc1);
    void powerDc2Changed(quint16 powerDc2);
    void powerDc2ReadFinished(quint16 powerDc2);
    void batVoltageCharge1Changed(float batVoltageCharge1);
    void batVoltageCharge1ReadFinished(float batVoltageCharge1);
    void batCurrentCharge1Changed(float batCurrentCharge1);
    void batCurrentCharge1ReadFinished(float batCurrentCharge1);
    void batPowerCharge1Changed(qint16 batPowerCharge1);
    void batPowerCharge1ReadFinished(qint16 batPowerCharge1);
    void bmsConnectStateChanged(quint16 bmsConnectState);
    void bmsConnectStateReadFinished(quint16 bmsConnectState);
    void temperatureBatChanged(qint16 temperatureBat);
    void temperatureBatReadFinished(qint16 temperatureBat);
    void feedinPowerChanged(qint32 feedinPower);
    void feedinPowerReadFinished(qint32 feedinPower);
    void feedinEnergyTotalChanged(float feedinEnergyTotal);
    void feedinEnergyTotalReadFinished(float feedinEnergyTotal);
    void consumEnergyTotalChanged(float consumEnergyTotal);
    void consumEnergyTotalReadFinished(float consumEnergyTotal);
    void gridVoltageRChanged(float gridVoltageR);
    void gridVoltageRReadFinished(float gridVoltageR);
    void gridCurrentRChanged(float gridCurrentR);
    void gridCurrentRReadFinished(float gridCurrentR);
    void gridPowerRChanged(qint16 gridPowerR);
    void gridPowerRReadFinished(qint16 gridPowerR);
    void gridFrequencyRChanged(float gridFrequencyR);
    void gridFrequencyRReadFinished(float gridFrequencyR);
    void gridVoltageSChanged(float gridVoltageS);
    void gridVoltageSReadFinished(float gridVoltageS);
    void gridCurrentSChanged(float gridCurrentS);
    void gridCurrentSReadFinished(float gridCurrentS);
    void gridPowerSChanged(qint16 gridPowerS);
    void gridPowerSReadFinished(qint16 gridPowerS);
    void gridFrequencySChanged(float gridFrequencyS);
    void gridFrequencySReadFinished(float gridFrequencyS);
    void gridVoltageTChanged(float gridVoltageT);
    void gridVoltageTReadFinished(float gridVoltageT);
    void gridCurrentTChanged(float gridCurrentT);
    void gridCurrentTReadFinished(float gridCurrentT);
    void gridPowerTChanged(qint16 gridPowerT);
    void gridPowerTReadFinished(qint16 gridPowerT);
    void gridFrequencyTChanged(float gridFrequencyT);
    void gridFrequencyTReadFinished(float gridFrequencyT);
    void solarEnergyTotalChanged(float solarEnergyTotal);
    void solarEnergyTotalReadFinished(float solarEnergyTotal);
    void solarEnergyTodayChanged(float solarEnergyToday);
    void solarEnergyTodayReadFinished(float solarEnergyToday);

protected:
    quint16 m_unlockPassword;
    quint16 m_batteryCapacity = 0;
    quint16 m_bmsWarningLsb = 0;
    quint16 m_bmsWarningMsb = 0;
    quint32 m_inverterFaultBits = 0;
    quint16 m_meter1CommunicationState = 0;
    float m_readExportLimit = 0;
    float m_writeExportLimit = 0;
    quint16 m_firmwareVersion = 0;
    quint16 m_inverterType = 0;
    QString m_factoryName;
    QString m_moduleName;
    float m_inverterVoltage = 0;
    float m_inverterCurrent = 0;
    qint16 m_inverterPower = 0;
    float m_pvVoltage1 = 0;
    float m_pvVoltage2 = 0;
    float m_pvCurrent1 = 0;
    float m_pvCurrent2 = 0;
    float m_inverterFrequency = 0;
    qint16 m_temperature = 0;
    RunMode m_runMode = RunModeWaitMode;
    quint16 m_powerDc1 = 0;
    quint16 m_powerDc2 = 0;
    float m_batVoltageCharge1 = 0;
    float m_batCurrentCharge1 = 0;
    qint16 m_batPowerCharge1 = 0;
    quint16 m_bmsConnectState = 0;
    qint16 m_temperatureBat = 0;
    qint32 m_feedinPower = 0;
    float m_feedinEnergyTotal = 0;
    float m_consumEnergyTotal = 0;
    float m_gridVoltageR = 0;
    float m_gridCurrentR = 0;
    qint16 m_gridPowerR = 0;
    float m_gridFrequencyR = 0;
    float m_gridVoltageS = 0;
    float m_gridCurrentS = 0;
    qint16 m_gridPowerS = 0;
    float m_gridFrequencyS = 0;
    float m_gridVoltageT = 0;
    float m_gridCurrentT = 0;
    qint16 m_gridPowerT = 0;
    float m_gridFrequencyT = 0;
    float m_solarEnergyTotal = 0;
    float m_solarEnergyToday = 0;
    quint32 m_modeType = 0;
    quint32 m_pvPowerLimit = 0;
    quint32 m_forceBatteryPower = 0;
    quint32 m_batteryTimeout = 0;

    void processBatteryCapacityRegisterValues(const QVector<quint16> values);
    void processBmsWarningLsbRegisterValues(const QVector<quint16> values);
    void processBmsWarningMsbRegisterValues(const QVector<quint16> values);
    void processInverterFaultBitsRegisterValues(const QVector<quint16> values);
    void processMeter1CommunicationStateRegisterValues(const QVector<quint16> values);
    void processReadExportLimitRegisterValues(const QVector<quint16> values);
    void processFirmwareVersionRegisterValues(const QVector<quint16> values);
    void processInverterTypeRegisterValues(const QVector<quint16> values);

    void processFactoryNameRegisterValues(const QVector<quint16> values);
    void processModuleNameRegisterValues(const QVector<quint16> values);

    void processInverterVoltageRegisterValues(const QVector<quint16> values);
    void processInverterCurrentRegisterValues(const QVector<quint16> values);
    void processInverterPowerRegisterValues(const QVector<quint16> values);
    void processPvVoltage1RegisterValues(const QVector<quint16> values);
    void processPvVoltage2RegisterValues(const QVector<quint16> values);
    void processPvCurrent1RegisterValues(const QVector<quint16> values);
    void processPvCurrent2RegisterValues(const QVector<quint16> values);
    void processInverterFrequencyRegisterValues(const QVector<quint16> values);
    void processTemperatureRegisterValues(const QVector<quint16> values);
    void processRunModeRegisterValues(const QVector<quint16> values);
    void processPowerDc1RegisterValues(const QVector<quint16> values);
    void processPowerDc2RegisterValues(const QVector<quint16> values);

    void processBatVoltageCharge1RegisterValues(const QVector<quint16> values);
    void processBatCurrentCharge1RegisterValues(const QVector<quint16> values);
    void processBatPowerCharge1RegisterValues(const QVector<quint16> values);
    void processBmsConnectStateRegisterValues(const QVector<quint16> values);
    void processTemperatureBatRegisterValues(const QVector<quint16> values);

    void processFeedinPowerRegisterValues(const QVector<quint16> values);
    void processFeedinEnergyTotalRegisterValues(const QVector<quint16> values);
    void processConsumEnergyTotalRegisterValues(const QVector<quint16> values);

    void processGridVoltageRRegisterValues(const QVector<quint16> values);
    void processGridCurrentRRegisterValues(const QVector<quint16> values);
    void processGridPowerRRegisterValues(const QVector<quint16> values);
    void processGridFrequencyRRegisterValues(const QVector<quint16> values);
    void processGridVoltageSRegisterValues(const QVector<quint16> values);
    void processGridCurrentSRegisterValues(const QVector<quint16> values);
    void processGridPowerSRegisterValues(const QVector<quint16> values);
    void processGridFrequencySRegisterValues(const QVector<quint16> values);
    void processGridVoltageTRegisterValues(const QVector<quint16> values);
    void processGridCurrentTRegisterValues(const QVector<quint16> values);
    void processGridPowerTRegisterValues(const QVector<quint16> values);
    void processGridFrequencyTRegisterValues(const QVector<quint16> values);

    void processSolarEnergyTotalRegisterValues(const QVector<quint16> values);
    void processSolarEnergyTodayRegisterValues(const QVector<quint16> values);

    void handleModbusError(QModbusDevice::Error error);
    void testReachability();

private:
    ModbusDataUtils::ByteOrder m_endianness = ModbusDataUtils::ByteOrderLittleEndian;
    quint16 m_slaveId = 1;

    bool m_reachable = false;
    QModbusReply *m_checkRechableReply = nullptr;
    uint m_checkReachableRetries = 0;
    uint m_checkReachableRetriesCount = 0;
    bool m_communicationWorking = false;
    quint8 m_communicationFailedMax = 15;
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

QDebug operator<<(QDebug debug, SolaxModbusTcpConnection *solaxModbusTcpConnection);

#endif // SOLAXMODBUSTCPCONNECTION_H
