#include "LogViewerViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QPointer>

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

    QPointer<LogViewerViewModel> self(this);

    (void)QtConcurrent::run([self, cmd, stopFlag, fsPtr] {
        fsPtr->streamCommand(cmd, [stopFlag, self](const std::string& line) {
            if (stopFlag->load(std::memory_order_acquire)) return;
            if (!self) return;
            // Safe if the view-model is gone: QPointer is cleared on QObject
            // destruction, and the queued functor re-checks before emitting.
            QMetaObject::invokeMethod(self, [self, qline = QString::fromStdString(line)] {
                if (self) emit self->logLineReceived(qline);
            }, Qt::QueuedConnection);
        });
        if (!self) return;
        QMetaObject::invokeMethod(self, [self] {
            if (!self) return;
            self->setStreaming(false);
            self->setStatus("Stream ended.");
            emit self->streamEnded();
        }, Qt::QueuedConnection);
    });
}

void LogViewerViewModel::stopStream()
{
    if (stopFlag_) stopFlag_->store(true, std::memory_order_release);
    fs_.cancelStreamingCommand();
    setStreaming(false);
    setStatus("Stream stopped.");
}

} // namespace OpenC3::ViewModels
