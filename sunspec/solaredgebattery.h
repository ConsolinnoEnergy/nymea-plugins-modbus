/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
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

#ifndef SOLAREDGEBATTERY_H
#define SOLAREDGEBATTERY_H

#include <QObject>
#include <QDebug>
#include <QTimer>

#include "sunspecthing.h"

/* Note: this is actually not a sunspec thing, since SolarEdge provide battery information using a non SunSpec modbus register section.
 * For keeping the code as simple as possible we make it a SunSpec thing */

class SolarEdgeBattery : public SunSpecThing
{
    Q_OBJECT
public:
    enum BatteryRegister {
        ManufacturerName = 0x00,
        Model = 0x10,
        FirmwareVersion = 0x20,
        SerialNumber = 0x30,
        BatteryDeviceId = 0x40,
        RatedEnergy = 0x42, // W * H
        MaxChargeContinuesPower = 0x44, // W
        MaxDischargeContinuesPower = 0x46, // W
        MaxChargePeakPower = 0x48, // W
        MaxDischargePeakPower = 0x4A, // W
        // 32 byte reserved
        BatteryAverageTemperature = 0x6c, // °C
        BatteryMaxTemperature = 0x6E,
        InstantaneousVoltage = 0x70, // V
        InstantaneousCurrent = 0x72, // A
        InstantaneousPower = 0x74, // W
        LifetimeExportEnergyCounter = 0x76, // W * H
        LifetimeImportEnergyCounter = 0x7A, // W * H
        MaxEnergy = 0x7E, // W * H
        AvailableEnergy = 0x80, // W * H
        StateOfHealth = 0x82,
        StateOfEnergy = 0x84,
        Status = 0x86
    };
    Q_ENUM(BatteryRegister)

    enum BatteryStatus {
        Off = 0,
        Standby = 1,
        Init = 2,
        Charge = 3,
        Discharge = 4,
        Fault = 5,
        Holding = 6,
        Idle = 7
    };
    Q_ENUM(BatteryStatus)

    enum StorageAcChargePolicy {
        Disable = 0,
        AlwaysAllowed = 1,
        FixedEnergyLimit = 2,
        PercentOfProduction = 3
    };
    Q_ENUM(StorageAcChargePolicy)

    enum BatteryControlMode {
        ControlDisabled = 0,
        MaximizeSelfConsumption = 1,
        TimeOfUse = 2,
        BackupOnly = 3,
        RemoteControl = 4
    };
    Q_ENUM(BatteryControlMode)

    enum StorageChargeDischargeMode {
        ChargeDischargeOff = 0,
        ChargeExcessPvPowerOnly = 1,
        ChargeFromPvFirst = 2,
        ChargeFromPvAndAc = 3,
        MaximizeExport = 4,
        DischargeToMeetLoads = 5,
        MaximizeSelfConsumptionMode = 7
    };
    Q_ENUM(StorageChargeDischargeMode)

    struct BatteryData {
        QString manufacturerName;
        QString model;
        QString firmwareVersion;
        QString serialNumber;
        quint16 batteryDeviceId;
        float ratedEnergy;
        float maxChargeContinuesPower;
        float maxDischargeContinuesPower;
        float maxChargePeakPower;
        float maxDischargePeakPower;
        float averageTemperature;
        float maxTemperature;
        float instantaneousVoltage;
        float instantaneousCurrent;
        float instantaneousPower;
        quint64 lifetimeExportEnergyCounter;
        quint64 lifetimeImportEnergyCounter;
        float maxEnergy;
        float availableEnergy;
        float stateOfHealth;
        float stateOfEnergy;
        BatteryStatus batteryStatus;
    };

    explicit SolarEdgeBattery(Thing *thing, SunSpecConnection *connection, int modbusStartRegister, QObject *parent = nullptr);

    SunSpecConnection *connection() const override;

    quint16 modbusStartRegister() const override;

    BatteryData batteryData() const;

    void init();
    void readBlockData() override;

    QModbusReply* activateRemoteControl(bool activate);
    QModbusReply* manualChargeDischarge(double power, int timeout);

signals:
    void initFinished(bool success);
    void blockDataUpdated();
    void manualModeChanged(bool active);
    void chargePowerChanged(double power);
    void timeoutChanged(int seconds);
    void chargeDischargePowerChanged(double power);
    void chargeDischargeDurationChanged(int duration);

private:
    QTimer m_timer;
    SunSpecConnection *m_connection = nullptr;
    int m_modbusStartRegister;
    bool m_initFinishedSuccess = false;
    BatteryData m_batteryData;
    StorageAcChargePolicy m_defaultAcChargePolicy = AlwaysAllowed;
    BatteryControlMode m_controlMode = MaximizeSelfConsumption;
    StorageChargeDischargeMode m_defaultChargeDischargeMode = MaximizeSelfConsumptionMode;
    quint32 m_remoteControlTimeout = 3600;
    float m_remoteControlChargePower = 1000; // Replaced qfloat32 with float
    float m_remoteControlDischargePower = 1000; // Replaced qfloat32 with float
    const quint16 StorageControlModeRegister = 0xE004;
    const quint16 StorageAcChargePolicyRegister = 0xE005;
    const quint16 StorageAcChargeLimitRegister = 0xE006;
    const quint16 StorageBackupReservedSettingRegister = 0xE008;
    const quint16 StorageChargeDischargeDefaultModeRegister = 0xE00A;
    const quint16 RemoteControlCommandTimeoutRegister = 0xE00B;
    const quint16 RemoteControlCommandModeRegister = 0xE00D;
    const quint16 RemoteControlChargeLimitRegister = 0xE00E;
    const quint16 RemoteControlCommandDischargeLimitRegister = 0xE010;
};

QDebug operator<<(QDebug debug, const SolarEdgeBattery::BatteryData &batteryData);

#endif // SOLAREDGEBATTERY_H