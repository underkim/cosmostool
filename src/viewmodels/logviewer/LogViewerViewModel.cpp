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

    // A fresh flag per stream, kept alive by the shared_ptr copy captured
    // below - independent of this ViewModel's lifetime. stopStream() (and
    // the destructor) flip it through the member copy; the worker thread
    // only ever touches its own copy, never `this`.
    stopFlag_ = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> stopFlag = stopFlag_;

    // Raw pointer to the long-lived service object (owned elsewhere, well
    // beyond this ViewModel's lifetime), captured by value so the worker
    // thread never has to read `this->fs_` - unlike a captured reference to
    // a local variable, this pointer stays valid regardless of `this`.
    Services::IRemoteFileService* fsPtr = &fs_;

    setStatus("Streaming: " + command);
    const std::string cmd = command.toStdString();

    (void)QtConcurrent::run([this, cmd, stopFlag, fsPtr] {
        fsPtr->streamCommand(cmd, [stopFlag, this](const std::string& line) {
            if (stopFlag->load(std::memory_order_acquire)) return;
            // Safe even if `this` was destroyed by now: Qt's context-object
            // overload of invokeMethod only runs the functor if the context
            // (`this`) is still alive when the event is processed.
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
    if (stopFlag_) stopFlag_->store(true, std::memory_order_release);
    setStreaming(false);
    setStatus("Stream stopped.");
}

} // namespace OpenC3::ViewModels
