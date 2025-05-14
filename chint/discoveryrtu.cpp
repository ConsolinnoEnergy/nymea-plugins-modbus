#include "discoveryrtu.h"
#include "extern-plugininfo.h"

DiscoveryRtu::DiscoveryRtu(ModbusRtuHardwareResource *modbusRtuResource,
                           uint modbusId,
                           QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource},
    m_modbusId{modbusId},
    m_endianness{ModbusDataUtils::ByteOrderBigEndian}
{
}

void DiscoveryRtu::startDiscovery()
{
    qCInfo(dcChint()) << "Discovery: Searching for smartmeter on modbus RTU...";

    if (m_modbusRtuResource->modbusRtuMasters().isEmpty()) {
        qCWarning(dcChint()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
    }

    m_openReplies = 0;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->connected()) {
            tryConnect(master, m_modbusId);
        } else {
            qCWarning(dcChint()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }

    if (m_openReplies < 0) {
        m_openReplies = 0;
    }

    if (m_openReplies > 0) {
        qCDebug(dcChint()) << "Waiting for modbus RTU replies.";
        connect(this, &DiscoveryRtu::repliesFinished, this, [this](){
            emit discoveryFinished(true);
        });
    } else {
        emit discoveryFinished(true);
    }
}

QList<DiscoveryRtu::Result> DiscoveryRtu::discoveryResults() const
{
    return m_discoveryResults;
}

void DiscoveryRtu::tryConnect(ModbusRtuMaster *master, quint16 modbusId)
{
    qCDebug(dcChint()) << "Scanning modbus RTU master" << master->modbusUuid() << "Modbus ID:" << modbusId;
    qCDebug(dcChint()) << "Trying to read register 8260 (frequency) to test the connection.";

    m_openReplies++;
    ModbusRtuReply *reply = master->readHoldingRegister(modbusId, 8260, 2);
    connect(reply, &ModbusRtuReply::finished, this, [=]{
        m_openReplies--;
        qCDebug(dcChint()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() < 2) {
            qCDebug(dcChint()) << "Error reading input register 8260 (frequency). This is not a smartmeter.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }

        // Test frequency value.
        float frequency = ModbusDataUtils::convertToFloat32(reply->result(), m_endianness) / 100;
        if (frequency < 49.0 || frequency > 51.0) {
            qCDebug(dcChint())
                    << "Recieved value for frequency is"
                    << frequency
                    << "Hz. This does not seem to be the correct value. This is not a smartmeter.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }
        qCDebug(dcChint()) << "Recieved value for frequency is" << frequency << "Hz. Value seems ok.";

        Result result{ master->modbusUuid(), master->serialPort()};
        qCDebug(dcChint()) << "Found a smartmeter. Adding it to the list.";
        m_discoveryResults.append(result);

        if (m_openReplies <= 0) {
            emit repliesFinished();
        }

    });
}
