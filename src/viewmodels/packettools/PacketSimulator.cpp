#include "PacketSimulator.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

namespace OpenC3::ViewModels
{

PacketSimulator::PacketSimulator(QObject* parent) : QObject(parent) {}

PacketSimulator::~PacketSimulator()
{
    stop();
}

PacketSimulator::Mode PacketSimulator::mode() const noexcept
{
    return mode_;
}

bool PacketSimulator::isRunning() const noexcept
{
    return mode_ != Mode::Stopped;
}

bool PacketSimulator::startUdpListener(const QString& bindAddress, quint16 port)
{
    stop();

    udpSocket_ = new QUdpSocket(this);
    connect(udpSocket_, &QUdpSocket::readyRead, this, &PacketSimulator::readUdpDatagrams);

    const QHostAddress address = parseBindAddress(bindAddress);
    if (address.isNull())
    {
        const QString reason =
            QString("UDP bind failed: bind address '%1' is invalid.").arg(bindAddress);
        udpSocket_->deleteLater();
        udpSocket_ = nullptr;
        emit errorOccurred(reason);
        return false;
    }
    if (!udpSocket_->bind(address, port,
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        const QString reason = QString("UDP bind failed on %1:%2: %3")
                                   .arg(address.toString())
                                   .arg(port)
                                   .arg(udpSocket_->errorString());
        udpSocket_->deleteLater();
        udpSocket_ = nullptr;
        emit errorOccurred(reason);
        return false;
    }

    mode_ = Mode::UdpListener;
    emit started(
        QString("UDP listener active on %1:%2").arg(address.toString()).arg(port));
    return true;
}

bool PacketSimulator::startTcpServer(const QString& bindAddress, quint16 port)
{
    stop();

    tcpServer_ = new QTcpServer(this);
    connect(tcpServer_, &QTcpServer::newConnection, this,
            &PacketSimulator::acceptTcpClient);

    const QHostAddress address = parseBindAddress(bindAddress);
    if (address.isNull())
    {
        const QString reason =
            QString("TCP listen failed: bind address '%1' is invalid.").arg(bindAddress);
        tcpServer_->deleteLater();
        tcpServer_ = nullptr;
        emit errorOccurred(reason);
        return false;
    }
    if (!tcpServer_->listen(address, port))
    {
        const QString reason = QString("TCP listen failed on %1:%2: %3")
                                   .arg(address.toString())
                                   .arg(port)
                                   .arg(tcpServer_->errorString());
        tcpServer_->deleteLater();
        tcpServer_ = nullptr;
        emit errorOccurred(reason);
        return false;
    }

    mode_ = Mode::TcpServer;
    emit started(QString("TCP server active on %1:%2").arg(address.toString()).arg(port));
    return true;
}

void PacketSimulator::stop()
{
    clearTcpClients();

    if (tcpServer_)
    {
        tcpServer_->close();
        tcpServer_->deleteLater();
        tcpServer_ = nullptr;
    }
    if (udpSocket_)
    {
        udpSocket_->close();
        udpSocket_->deleteLater();
        udpSocket_ = nullptr;
    }

    const bool wasRunning = mode_ != Mode::Stopped;
    mode_ = Mode::Stopped;
    if (wasRunning)
        emit stopped();
}

bool PacketSimulator::sendUdpHex(const QString& host, quint16 port,
                                 const QString& hexPayload)
{
    QString error;
    const QByteArray payload = parseHexPayload(hexPayload, &error);
    if (!error.isEmpty())
    {
        emit errorOccurred(error);
        return false;
    }

    QUdpSocket socket;
    QHostAddress destination(host);
    if (destination.isNull() &&
        host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0)
    {
        destination = QHostAddress::LocalHost;
    }
    if (destination.isNull())
    {
        emit errorOccurred(
            QString(
                "UDP send failed: host '%1' is not a numeric IP address or localhost.")
                .arg(host));
        return false;
    }
    const qint64 written = socket.writeDatagram(payload, destination, port);
    if (written != payload.size())
    {
        emit errorOccurred(QString("UDP send failed to %1:%2: %3")
                               .arg(host)
                               .arg(port)
                               .arg(socket.errorString()));
        return false;
    }

    emit packetSent(QStringLiteral("UDP"), QString("%1:%2").arg(host).arg(port),
                    toHex(payload));
    return true;
}

bool PacketSimulator::sendTcpHex(const QString& hexPayload)
{
    QString error;
    const QByteArray payload = parseHexPayload(hexPayload, &error);
    if (!error.isEmpty())
    {
        emit errorOccurred(error);
        return false;
    }

    int sentCount = 0;
    for (const auto& clientRef : tcpClients_)
    {
        QTcpSocket* client = clientRef.data();
        if (!client || client->state() != QAbstractSocket::ConnectedState)
            continue;
        const qint64 written = client->write(payload);
        if (written == payload.size())
        {
            client->flush();
            ++sentCount;
        }
    }

    if (sentCount == 0)
    {
        emit errorOccurred(
            QStringLiteral("TCP send failed: no connected COSMOS client."));
        return false;
    }

    emit packetSent(QStringLiteral("TCP"), QString("%1 client(s)").arg(sentCount),
                    toHex(payload));
    return true;
}

void PacketSimulator::acceptTcpClient()
{
    if (!tcpServer_)
        return;

    while (tcpServer_->hasPendingConnections())
    {
        QTcpSocket* client = tcpServer_->nextPendingConnection();
        client->setParent(this);
        tcpClients_.append(QPointer<QTcpSocket>(client));
        connect(client, &QTcpSocket::readyRead, this, &PacketSimulator::readTcpClient);
        connect(client, &QTcpSocket::disconnected, client, &QTcpSocket::deleteLater);
        emit started(QString("TCP client connected: %1:%2")
                         .arg(client->peerAddress().toString())
                         .arg(client->peerPort()));
    }
}

void PacketSimulator::readTcpClient()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client)
        return;

    const QByteArray payload = client->readAll();
    emitReceived(
        QStringLiteral("TCP"),
        QString("%1:%2").arg(client->peerAddress().toString()).arg(client->peerPort()),
        payload);
}

void PacketSimulator::readUdpDatagrams()
{
    if (!udpSocket_)
        return;

    while (udpSocket_->hasPendingDatagrams())
    {
        QByteArray payload;
        payload.resize(static_cast<int>(udpSocket_->pendingDatagramSize()));
        QHostAddress senderAddress;
        quint16 senderPort = 0;
        const qint64 read = udpSocket_->readDatagram(payload.data(), payload.size(),
                                                     &senderAddress, &senderPort);
        if (read < 0)
        {
            emit errorOccurred(
                QString("UDP receive failed: %1").arg(udpSocket_->errorString()));
            continue;
        }
        payload.resize(static_cast<int>(read));
        emitReceived(QStringLiteral("UDP"),
                     QString("%1:%2").arg(senderAddress.toString()).arg(senderPort),
                     payload);
    }
}

QByteArray PacketSimulator::parseHexPayload(const QString& hexPayload, QString* error)
{
    QString normalized = hexPayload;
    normalized.remove(' ');
    normalized.remove('\t');
    normalized.remove('\n');
    normalized.remove('\r');
    normalized.remove(QStringLiteral("0x"), Qt::CaseInsensitive);

    if (normalized.isEmpty())
    {
        if (error)
            *error = QStringLiteral("Packet payload is empty.");
        return {};
    }
    if ((normalized.size() % 2) != 0)
    {
        if (error)
            *error = QStringLiteral("Hex payload must contain an even number of digits.");
        return {};
    }

    const QByteArray hex = normalized.toLatin1();
    const QByteArray bytes = QByteArray::fromHex(hex);
    if (bytes.toHex().toUpper() != hex.toUpper())
    {
        if (error)
            *error = QStringLiteral("Hex payload contains non-hex characters.");
        return {};
    }

    if (error)
        error->clear();
    return bytes;
}

QString PacketSimulator::toHex(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

QString PacketSimulator::toAsciiPreview(const QByteArray& bytes)
{
    QString preview;
    preview.reserve(bytes.size());
    for (const char byte : bytes)
    {
        const uchar value = static_cast<uchar>(byte);
        preview += (value >= 0x20 && value <= 0x7E) ? QChar(value) : QChar('.');
    }
    return preview;
}

QHostAddress PacketSimulator::parseBindAddress(const QString& bindAddress)
{
    const QString normalized = bindAddress.trimmed();
    if (normalized.isEmpty() || normalized == QStringLiteral("0.0.0.0") ||
        normalized == QStringLiteral("*"))
    {
        return QHostAddress::AnyIPv4;
    }
    if (normalized.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0)
    {
        return QHostAddress::LocalHost;
    }
    return QHostAddress(normalized);
}

void PacketSimulator::emitReceived(const QString& transport, const QString& peer,
                                   const QByteArray& payload)
{
    emit packetReceived(transport, peer, toHex(payload), toAsciiPreview(payload));
}

void PacketSimulator::clearTcpClients()
{
    for (const auto& clientRef : tcpClients_)
    {
        QTcpSocket* client = clientRef.data();
        if (!client)
            continue;
        client->disconnect(this);
        client->close();
        client->deleteLater();
    }
    tcpClients_.clear();
}

} // namespace OpenC3::ViewModels
