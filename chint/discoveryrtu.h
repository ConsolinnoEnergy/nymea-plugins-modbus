#ifndef DISCOVERYRTU_H
#define DISCOVERYRTU_H

#include <QObject>

#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <modbusdatautils.h>

class DiscoveryRtu : public QObject
{
    Q_OBJECT
public:
    explicit DiscoveryRtu(ModbusRtuHardwareResource *modbusRtuResource,
                          uint modbusId,
                          QObject *parent = nullptr);
    struct Result {
        QUuid modbusRtuMasterId;
        QString serialPort;
    };

    void startDiscovery();

    QList<Result> discoveryResults() const;

signals:
    void discoveryFinished(bool modbusRtuMasterAvailable);
    void repliesFinished();

private slots:
    void tryConnect(ModbusRtuMaster *master, quint16 modbusId);

private:
    ModbusRtuHardwareResource *m_modbusRtuResource = nullptr;
    uint m_modbusId;
    qint16 m_openReplies;
    ModbusDataUtils::ByteOrder m_endianness;

    QList<Result> m_discoveryResults;
};

#endif // DISCOVERYRTU_H
