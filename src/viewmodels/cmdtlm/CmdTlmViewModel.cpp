#include "CmdTlmViewModel.h"
#include "CmdTlmParser.h"
#include "core/connection/ShellQuote.h"
#include "core/logging/Logger.h"
#include "services/connection/IConnectionService.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>

namespace OpenC3::ViewModels {

CmdTlmViewModel::CmdTlmViewModel(
    Services::IConnectionService& connection,
    Services::IRemoteFileService& fs,
    QObject* parent)
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

QString CmdTlmViewModel::defaultCmdTlmPath() const
{
    QString root = QString::fromStdString(connection_.cosmosRootPath()).trimmed();
    while (root.size() > 1 && root.endsWith('/'))
        root.chop(1);

    if (root.endsWith("/openc3.sh", Qt::CaseInsensitive)) {
        const int slash = root.lastIndexOf('/');
        root = slash > 0 ? root.left(slash) : QString{};
    } else if (root.compare("openc3.sh", Qt::CaseInsensitive) == 0) {
        root.clear();
    }

    return root.isEmpty() ? QString("targets") : root + "/targets";
}

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
    setStatus("Listing " + remotePath);

    (void)QtConcurrent::run([this, path] {
        auto entries = fs_.listDirectory(path);
        QMetaObject::invokeMethod(this, [this, entries = std::move(entries),
                                          qpath = QString::fromStdString(path)]() mutable {
            QStringList list;
            list.reserve(static_cast<qsizetype>(entries.size()));
            for (const auto& e : entries)
                list << QString::fromStdString(e);
            emit directoryListed(list, qpath);
            setBusy(false);
            setStatus(QString("Found %1 entries").arg(list.size()));
        }, Qt::QueuedConnection);
    });
}

void CmdTlmViewModel::listPluginFiles(const QString& pluginRootPath)
{
    if (busy_) return;
    const std::string root = pluginRootPath.toStdString();
    setBusy(true);
    setStatus("Loading plugin files: " + pluginRootPath);

    (void)QtConcurrent::run([this, root] {
        using Core::Connection::shellQuote;
        const std::string command =
            "root=" + shellQuote(root) + "; "
            "[ -d \"$root\" ] || exit 0; "
            "find \"$root\" \\( "
            "-path '*/cmd_tlm/*' -o "
            "-path '*/screens/*' -o "
            "-path '*/procedures/*' -o "
            "-name 'plugin.txt' -o "
            "-name '*.gemspec' "
            "\\) -type f "
            "| sed \"s#^$root/##\" "
            "| sort";

        const std::string output = fs_.executeCommand(command);

        QMetaObject::invokeMethod(this,
            [this, output, qroot = QString::fromStdString(root)] {
                QStringList files;
                const QString qout = QString::fromStdString(output);
                for (const QString& line : qout.split('\n', Qt::SkipEmptyParts)) {
                    const QString trimmed = line.trimmed();
                    if (!trimmed.isEmpty())
                        files << trimmed;
                }
                emit pluginFilesListed(files, qroot);
                setBusy(false);
                setStatus(QString("Loaded %1 plugin file(s)").arg(files.size()));
            }, Qt::QueuedConnection);
    });
}

void CmdTlmViewModel::openFile(const QString& remotePath)
{
    if (busy_) return;
    const std::string path = remotePath.toStdString();
    setBusy(true);
    setStatus("Loading " + remotePath);

    (void)QtConcurrent::run([this, path] {
        const std::string content = fs_.readFile(path);
        QMetaObject::invokeMethod(this,
            [this, content, qpath = QString::fromStdString(path)] {
                const QString qcontent = QString::fromStdString(content);
                emit fileOpened(qpath, qcontent);
                setBusy(false);
                setStatus("Loaded " + qpath);
                if (qpath.endsWith(".txt", Qt::CaseInsensitive))
                    parseContent(qcontent, qpath);
            }, Qt::QueuedConnection);
    });
}

void CmdTlmViewModel::saveFile(const QString& remotePath, const QString& content)
{
    if (busy_) return;
    const std::string path = remotePath.toStdString();
    const std::string data = content.toStdString();
    setBusy(true);
    setStatus("Saving");

    (void)QtConcurrent::run([this, path, data] {
        const bool ok = fs_.writeFile(path, data);
        QMetaObject::invokeMethod(this, [this, ok, qpath = QString::fromStdString(path)] {
            emit fileSaved(qpath, ok);
            setBusy(false);
            setStatus(ok ? "Saved: " + qpath : "Save failed.");
        }, Qt::QueuedConnection);
    });
}

void CmdTlmViewModel::parseContent(const QString& content, const QString& filePath)
{
    const CmdTlmParseResult result = CmdTlmParser::parse(content);
    const int e = result.errorCount();
    const int w = result.warningCount();
    setStatus(QString("Parsed: %1 blocks, %2 error(s), %3 warning(s)")
                  .arg(result.blocks.size()).arg(e).arg(w));
    emit fileParsed(result, filePath);
}

} // namespace OpenC3::ViewModels
