#include "CmdTlmViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>

namespace OpenC3::ViewModels {

CmdTlmViewModel::CmdTlmViewModel(
    Services::IConnectionService&  connection,
    Services::IRemoteFileService&  fs,
    QObject*                       parent)
    : ViewModelBase(parent)
    , connection_(connection)
    , fs_(fs)
{
    connection_.onStateChanged([this](const Services::ConnectionEvent& ev) {
        Q_UNUSED(ev);
        QMetaObject::invokeMethod(this, [this] {
            emit connectionChanged();
        }, Qt::QueuedConnection);
    });
}

bool CmdTlmViewModel::isConnected() const noexcept
{
    return connection_.state() == Services::ConnectionState::Connected;
}

bool CmdTlmViewModel::isBusy() const noexcept { return busy_; }

QString CmdTlmViewModel::statusMessage() const noexcept { return status_; }

void CmdTlmViewModel::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void CmdTlmViewModel::setStatus(const QString& msg)
{
    status_ = msg;
    emit statusMessageChanged();
}

void CmdTlmViewModel::listDirectory(const QString& remotePath)
{
    if (busy_) return;
    const std::string path = remotePath.toStdString();
    setBusy(true);
    setStatus("Listing " + remotePath + "…");

    (void)QtConcurrent::run([this, path] {
        auto entries = fs_.listDirectory(path);
        QMetaObject::invokeMethod(this, [this, entries = std::move(entries), qpath = QString::fromStdString(path)] () mutable {
            QStringList list;
            list.reserve(static_cast<qsizetype>(entries.size()));
            for (const auto& e : entries)
                list << QString::fromStdString(e);
            emit directoryListed(list, qpath);
            setBusy(false);
            setStatus(QString("Listed %1 entries").arg(list.size()));
        }, Qt::QueuedConnection);
    });
}

void CmdTlmViewModel::openFile(const QString& remotePath)
{
    if (busy_) return;
    const std::string path = remotePath.toStdString();
    setBusy(true);
    setStatus("Loading " + remotePath + "…");

    (void)QtConcurrent::run([this, path] {
        const std::string content = fs_.readFile(path);
        QMetaObject::invokeMethod(this, [this, content, qpath = QString::fromStdString(path)] {
            emit fileOpened(qpath, QString::fromStdString(content));
            setBusy(false);
            setStatus("Loaded " + qpath);
        }, Qt::QueuedConnection);
    });
}

void CmdTlmViewModel::saveFile(const QString& remotePath, const QString& content)
{
    if (busy_) return;
    const std::string path = remotePath.toStdString();
    const std::string data = content.toStdString();
    setBusy(true);
    setStatus("Saving…");

    (void)QtConcurrent::run([this, path, data] {
        const bool ok = fs_.writeFile(path, data);
        QMetaObject::invokeMethod(this, [this, ok, qpath = QString::fromStdString(path)] {
            emit fileSaved(qpath, ok);
            setBusy(false);
            setStatus(ok ? "Saved: " + qpath : "Save failed.");
        }, Qt::QueuedConnection);
    });
}

} // namespace OpenC3::ViewModels
