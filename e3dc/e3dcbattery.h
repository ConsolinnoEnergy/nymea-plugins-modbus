#ifndef E3DCBATTERY_H
#define E3DCBATTERY_H


#include <QObject>

class e3dcBattery : public QObject
{
    Q_OBJECT
public:

    enum Registers {
        RegisterCurrentPower = 40070,
        RegisterSOC = 40083
    };
    Q_ENUM(Registers);

    e3dcBattery();

    float currentPower();
    void setCurrentPower(float currentPower);

    int SOC();
    void setSOC(int SOC);

    int batteryID();
    void setBatteryID(int ID);

signals:
    void updated();

    void currentPowerChanged(float currentPower);
    void SOCChanged(int SOC);

private:

    float m_currentPower = 0;
    int m_SOC = 0;
    Registers m_current_Power_Register = RegisterCurrentPower;
    Registers m_network_Point_Power_Register = RegisterSOC;

    int m_batteryID;



};

#endif // E3DCBATTERY_H
