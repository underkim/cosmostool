#include "LogViewerViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>

namespace OpenC3::ViewModels {

LogViewerViewModel::LogViewerViewModel(
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
            if (!isConnected() && streaming_) stopStream();
            emit connectionChanged();
        }, Qt::QueuedConnection);
    });
}

LogViewerViewModel::~LogViewerViewModel()
{
    stopStream();
}

bool LogViewerViewModel::isConnected() const noexcept
{
    return connection_.state() == Services::ConnectionState::Connected;
}

bool LogViewerViewModel::isStreaming() const noexcept { return streaming_; }

QString LogViewerViewModel::statusMessage() const noexcept { return status_; }

void LogViewerViewModel::setStreaming(bool s)
{
    if (streaming_ == s) return;
    streaming_ = s;
    emit streamingChanged();
}

void LogViewerViewModel::setStatus(const QString& msg)
{
    status_ = msg;
    emit statusMessageChanged();
}

void LogViewerViewModel::startStream(const QString& command)
{
    if (streaming_ || !isConnected()) return;
    setStreaming(true);
    stopFlag_.storeRelease(0);
    setStatus("Streaming: " + command);

    const std::string cmd = command.toStdString();

    (void)QtConcurrent::run([this, cmd] {
        fs_.streamCommand(cmd, [this](const std::string& line) {
            if (stopFlag_.loadAcquire()) return;
            QMetaObject::invokeMethod(this, [this, qline = QString::fromStdString(line)] {
                emit logLineReceived(qline);
            }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(this, [this] {
            setStreaming(false);
            setStatus("Stream ended.");
            emit streamEnded();
        }, Qt::QueuedConnection);
    });
}

void LogViewerViewModel::stopStream()
{
    stopFlag_.storeRelease(1);
    setStreaming(false);
    setStatus("Stream stopped.");
}

} // namespace OpenC3::ViewModels
