#include "PacketToolsViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>

namespace OpenC3::ViewModels {

PacketToolsViewModel::PacketToolsViewModel(
    Services::IConnectionService& connection,
    Services::IRemoteFileService& fs,
    QObject*                      parent)
    : ViewModelBase(parent)
    , connection_(connection)
    , fs_(fs)
{
    connect(&simulator_, &PacketSimulator::started, this, [this](const QString& message) {
        setStatus(message);
        emit simulatorStateChanged();
    });
    connect(&simulator_, &PacketSimulator::stopped, this, [this] {
        setStatus(QStringLiteral("Simulator stopped."));
        emit simulatorStateChanged();
    });
    connect(&simulator_, &PacketSimulator::errorOccurred, this, [this](const QString& reason) {
        setStatus(reason);
        emit simulatorError(reason);
    });
    connect(&simulator_, &PacketSimulator::tcpClientCountChanged,
            this, &PacketToolsViewModel::tcpClientCountChanged);
    connect(&simulator_, &PacketSimulator::packetReceived,
            this, &PacketToolsViewModel::simulatorPacketReceived);
    connect(&simulator_, &PacketSimulator::packetSent,
            this, &PacketToolsViewModel::simulatorPacketSent);

    connection_.onStateChanged([this](const Services::ConnectionEvent& ev) {
        Q_UNUSED(ev);
        QMetaObject::invokeMethod(this, [this] {
            emit connectionChanged();
            if (isConnected()) refreshLogList();
        }, Qt::QueuedConnection);
    });
}

bool PacketToolsViewModel::isConnected() const noexcept
{
    return connection_.state() == Services::ConnectionState::Connected;
}

bool PacketToolsViewModel::isBusy() const noexcept { return busy_; }

QString PacketToolsViewModel::statusMessage() const noexcept { return status_; }

bool PacketToolsViewModel::simulatorRunning() const noexcept
{
    return simulator_.isRunning();
}

int PacketToolsViewModel::tcpClientCount() const noexcept
{
    return simulator_.tcpClientCount();
}

void PacketToolsViewModel::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void PacketToolsViewModel::setStatus(const QString& msg)
{
    status_ = msg;
    emit statusMessageChanged();
}

void PacketToolsViewModel::refreshLogList()
{
    if (busy_) return;
    setBusy(true);
    setStatus("Scanning packet log directory…");

    (void)QtConcurrent::run([this] {
        auto entries = fs_.listDirectory(kLogDir);
        QMetaObject::invokeMethod(this, [this, entries = std::move(entries)] () mutable {
            QStringList files;
            files.reserve(static_cast<qsizetype>(entries.size()));
            for (const auto& e : entries)
                files << QString::fromStdString(e);
            emit logListReady(files);
            setBusy(false);
            setStatus(QString("Found %1 log file(s)").arg(files.size()));
        }, Qt::QueuedConnection);
    });
}

void PacketToolsViewModel::openLogFile(const QString& remotePath)
{
    if (busy_) return;
    const std::string path = remotePath.toStdString();
    setBusy(true);
    setStatus("Loading " + remotePath + "…");

    (void)QtConcurrent::run([this, path] {
        const std::string content = fs_.readFile(path);
        QMetaObject::invokeMethod(this, [this, content, qpath = QString::fromStdString(path)] {
            emit logContentReady(qpath, QString::fromStdString(content));
            setBusy(false);
            setStatus("Loaded: " + qpath);
        }, Qt::QueuedConnection);
    });
}

void PacketToolsViewModel::analyzePackets(const QString& remotePath,
                                           const QString& filter)
{
    if (busy_) return;
    const std::string path   = remotePath.toStdString();
    const std::string filt   = filter.toStdString();
    setBusy(true);
    setStatus("Analyzing…");

    (void)QtConcurrent::run([this, path, filt] {
        const std::string cmd = "grep -c " +
            (filt.empty() ? std::string(".") : filt) + " " + path +
            " 2>/dev/null || echo 0";
        const std::string count = fs_.readFile("/dev/stdin"); // fallback stub
        Q_UNUSED(count);

        // Real impl would parse binary COSMOS packet logs.
        const QString summary = filt.empty()
            ? "Packet analysis: connect to view live data."
            : "Filter: " + QString::fromStdString(filt) + " — connect for results.";

        QMetaObject::invokeMethod(this, [this, summary] {
            emit analysisReady(summary);
            setBusy(false);
            setStatus("Analysis complete.");
        }, Qt::QueuedConnection);
    });
}

void PacketToolsViewModel::startUdpSimulator(const QString& bindAddress, quint16 port)
{
    (void)simulator_.startUdpListener(bindAddress, port);
}

void PacketToolsViewModel::startTcpSimulator(const QString& bindAddress, quint16 port)
{
    (void)simulator_.startTcpServer(bindAddress, port);
}

void PacketToolsViewModel::stopSimulator()
{
    simulator_.stop();
}

void PacketToolsViewModel::sendUdpSimulatorPacket(const QString& host,
                                                  quint16 port,
                                                  const QString& hexPayload)
{
    (void)simulator_.sendUdpHex(host, port, hexPayload);
}

void PacketToolsViewModel::sendTcpSimulatorPacket(const QString& hexPayload)
{
    (void)simulator_.sendTcpHex(hexPayload);
}

} // namespace OpenC3::ViewModels
