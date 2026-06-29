#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

namespace OpenC3::ViewModels
{

class PacketSimulator final : public QObject
{
    Q_OBJECT

  public:
    enum class Mode
    {
        Stopped,
        UdpListener,
        TcpServer
    };
    Q_ENUM(Mode)

    explicit PacketSimulator(QObject* parent = nullptr);
    ~PacketSimulator() override;

    [[nodiscard]] Mode mode() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

  public slots:
    bool startUdpListener(const QString& bindAddress, quint16 port);
    bool startTcpServer(const QString& bindAddress, quint16 port);
    void stop();

    bool sendUdpHex(const QString& host, quint16 port, const QString& hexPayload);
    bool sendTcpHex(const QString& hexPayload);

  signals:
    void started(const QString& description);
    void stopped();
    void errorOccurred(const QString& reason);
    void packetReceived(const QString& transport, const QString& peer,
                        const QString& hexPayload, const QString& asciiPreview);
    void packetSent(const QString& transport, const QString& peer,
                    const QString& hexPayload);

  private slots:
    void acceptTcpClient();
    void readTcpClient();
    void readUdpDatagrams();

  private:
    [[nodiscard]] static QByteArray parseHexPayload(const QString& hexPayload,
                                                    QString* error);
    [[nodiscard]] static QString toHex(const QByteArray& bytes);
    [[nodiscard]] static QString toAsciiPreview(const QByteArray& bytes);
    [[nodiscard]] static QHostAddress parseBindAddress(const QString& bindAddress);

    void emitReceived(const QString& transport, const QString& peer,
                      const QByteArray& payload);
    void clearTcpClients();

    Mode mode_{Mode::Stopped};
    QUdpSocket* udpSocket_{nullptr};
    QTcpServer* tcpServer_{nullptr};
    QVector<QPointer<QTcpSocket>> tcpClients_;
};

} // namespace OpenC3::ViewModels
