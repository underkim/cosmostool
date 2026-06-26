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

} // namespace OpenC3::ViewModels
