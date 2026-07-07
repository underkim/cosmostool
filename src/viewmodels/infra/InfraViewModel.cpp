#include "InfraViewModel.h"
#include "viewmodels/plugin/PluginTemplateEngine.h"
#include "core/connection/ShellQuote.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QStringList>
#include <QMap>

namespace OpenC3::ViewModels {

// ── Construction ──────────────────────────────────────────────────────────────

InfraViewModel::InfraViewModel(
    Services::IConnectionService& connection,
    Services::IRemoteFileService& fs,
    QObject*                      parent)
    : ViewModelBase(parent)
    , connection_(connection)
    , fs_(fs)
{
    connection_.onStateChanged([this](const Services::ConnectionEvent& ev) {
        const bool c = (ev.state == Services::ConnectionState::Connected);
        QMetaObject::invokeMethod(this, [this, c] {
            connected_ = c;
            emit connectionChanged();
        }, Qt::QueuedConnection);
    });
}

// ── Properties ────────────────────────────────────────────────────────────────

bool    InfraViewModel::isConnected()   const noexcept { return connected_; }
bool    InfraViewModel::isBusy()        const noexcept { return busy_; }
QString InfraViewModel::statusMessage() const noexcept { return statusMessage_; }

QString InfraViewModel::cosmosRootPath() const noexcept
{
    return infraRootPath();
}

QString InfraViewModel::defaultEnvPath()    const noexcept
{
    return infraRootPath() + "/.env";
}

QString InfraViewModel::defaultComposePath() const noexcept
{
    return infraRootPath() + "/compose.yaml";
}

QString InfraViewModel::defaultPluginsPath() const noexcept
{
    return infraRootPath() + "/plugins";
}

// ── Private helpers ───────────────────────────────────────────────────────────

QString InfraViewModel::infraRootPath() const noexcept
{
    QString path = QString::fromStdString(connection_.cosmosRootPath()).trimmed();
    if (path.isEmpty()) return "/cosmos";

    while (path.size() > 1 && path.endsWith('/'))
        path.chop(1);

    if (path.endsWith("/openc3.sh") || path == "openc3.sh") {
        const qsizetype slash = path.lastIndexOf('/');
        return slash > 0 ? path.left(slash) : ".";
    }

    return path;
}

void InfraViewModel::setStatus(const QString& msg)
{
    statusMessage_ = msg;
    emit statusMessageChanged();
}

void InfraViewModel::setBusy(bool b)
{
    busy_ = b;
    emit busyChanged();
}

// ── File operations ───────────────────────────────────────────────────────────

void InfraViewModel::loadEnvFile(const QString& remotePath)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Loading " + remotePath + " …");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this, path = remotePath.toStdString()] {
        const std::string content = fs_.readFile(path);
        const bool ok = !content.empty();
        QMetaObject::invokeMethod(this, [this,
            p   = QString::fromStdString(path),
            txt = QString::fromStdString(content), ok] {
                setBusy(false);
                setStatus(ok ? "Loaded: " + p : "File not found: " + p);
                if (ok) emit envLoaded(p, txt);
            }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::saveEnvFile(const QString& remotePath, const QString& content)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Saving …");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        path = remotePath.toStdString(),
        data = content.toStdString()] {
            const bool ok = fs_.writeFile(path, data);
            QMetaObject::invokeMethod(this, [this, p = QString::fromStdString(path), ok] {
                setBusy(false);
                setStatus(ok ? "Saved: " + p : "Save failed: " + p);
                emit fileSaved(p, ok);
            }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::loadComposeFile(const QString& remotePath)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Loading " + remotePath + " …");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this, path = remotePath.toStdString()] {
        const std::string content = fs_.readFile(path);
        const bool ok = !content.empty();
        QMetaObject::invokeMethod(this, [this,
            p   = QString::fromStdString(path),
            txt = QString::fromStdString(content), ok] {
                setBusy(false);
                setStatus(ok ? "Loaded: " + p : "File not found: " + p);
                if (ok) emit composeLoaded(p, txt);
            }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::saveComposeFile(const QString& remotePath, const QString& content)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        path = remotePath.toStdString(),
        data = content.toStdString()] {
            const bool ok = fs_.writeFile(path, data);
            QMetaObject::invokeMethod(this, [this, p = QString::fromStdString(path), ok] {
                setBusy(false);
                setStatus(ok ? "Saved: " + p : "Save failed: " + p);
                emit fileSaved(p, ok);
            }, Qt::QueuedConnection);
    }));
}

// ── Volume override workflow ──────────────────────────────────────────────────

void InfraViewModel::loadContainers()
{
    if (!connected_) { setStatus("Not connected"); return; }

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this] {
        const std::string out =
            fs_.executeCommand("docker ps --format '{{.Names}}'");
        QStringList names;
        for (const QString& line :
             QString::fromStdString(out).split('\n', Qt::SkipEmptyParts))
        {
            const QString n = line.trimmed();
            if (!n.isEmpty()) names << n;
        }
        QMetaObject::invokeMethod(this, [this, names] {
            emit containersLoaded(names);
        }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::extractContainerFile(
    const QString& container, const QString& filePath)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Extracting container file…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        ctr  = container.toStdString(),
        path = filePath.toStdString()] {
            const std::string content =
                fs_.executeCommand("docker exec " + Core::Connection::shellQuote(ctr)
                                   + " cat " + Core::Connection::shellQuote(path));
            const bool ok = !content.empty();
            QMetaObject::invokeMethod(this, [this,
                fpath = QString::fromStdString(path),
                txt   = QString::fromStdString(content), ok] {
                    setBusy(false);
                    setStatus(ok ? "File extracted" : "File not found");
                    if (ok) emit containerFileExtracted(fpath, txt);
                }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::applyVolumeOverride(
    const QString& container,
    const QString& containerFilePath,
    const QString& hostSavePath,
    const QString& content)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Saving volume override file…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        ctr      = container.toStdString(),
        ctrPath  = containerFilePath.toStdString(),
        hostPath = hostSavePath.toStdString(),
        data     = content.toStdString()] {

            // Unlike writePluginFiles()'s fullPath (always pluginDir + "/" +
            // relativePath, so rfind('/') can never miss), hostPath here is a
            // single free-text field the user can type without any
            // separator at all (e.g. "override.yaml") - substr(0, npos)
            // would then return the *whole* path as dirPath, mkdir'ing a
            // directory at the exact spot the file itself needs to go and
            // making the writeFile() below fail.
            const auto        slash   = hostPath.rfind('/');
            const std::string dirPath = (slash == std::string::npos) ? std::string{}
                                                                       : hostPath.substr(0, slash);
            [[maybe_unused]] const std::string mkdirOut = dirPath.empty()
                ? std::string{}
                : fs_.executeCommand("mkdir -p " + Core::Connection::shellQuote(dirPath));

            const bool ok = fs_.writeFile(hostPath, data);

            // compose.yaml volume entry: uses 6-space indent (service.volumes list item)
            const QString entry = QString("      - %1:%2")
                .arg(QString::fromStdString(hostPath))
                .arg(QString::fromStdString(ctrPath));
            // Helpful restart hint
            const QString hint = QString(
                "# Container: %1\n"
                "# Add the following entry to that service's volumes: section in compose.yaml:\n"
                "%2\n\n"
                "# Apply: docker compose up -d %1")
                .arg(QString::fromStdString(ctr))
                .arg(entry);

            QMetaObject::invokeMethod(this, [this, ok,
                hp   = QString::fromStdString(hostPath),
                hint] {
                    setBusy(false);
                    setStatus(ok ? "Saved — add the volume entry to compose.yaml"
                                 : "Save failed: " + hp);
                    if (ok) emit volumeEntryReady(hint);
                    emit overrideApplied(ok, hp);
                }, Qt::QueuedConnection);
    }));
}

// ── Plugin scaffolding ────────────────────────────────────────────────────────

void InfraViewModel::writePluginFiles(const QString& pluginDir, const QMap<QString, QString>& files)
{
    int created = 0;
    QStringList failed;

    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const std::string fullPath = (pluginDir + "/" + it.key()).toStdString();

        // Ensure parent directory exists on the remote host.
        // executeCommand returns stdout; mkdir -p errors go to stderr so
        // we rely on writeFile below to surface any actual failure.
        const std::string dirPath = fullPath.substr(0, fullPath.rfind('/'));
        [[maybe_unused]] const std::string mkdirOut =
            fs_.executeCommand("mkdir -p " + Core::Connection::shellQuote(dirPath));

        if (fs_.writeFile(fullPath, it.value().toStdString()))
            ++created;
        else
            failed << it.key();
    }

    const bool ok = failed.isEmpty();
    const QString detail = ok
        ? QString("%1 file(s) created (%2)")
              .arg(created).arg(pluginDir)
        : QString("Failed: ") + failed.join(", ");

    QMetaObject::invokeMethod(this, [this, pluginDir, ok, detail] {
        setBusy(false);
        setStatus(detail);
        emit scaffoldComplete(pluginDir, ok, detail);
    }, Qt::QueuedConnection);
}

void InfraViewModel::scaffoldPlugin(
    const QString& remoteRoot,
    const QString& pluginName,
    const QString& targetName,
    const QString& pluginNamespace,
    const QString& description,
    int            templateType,
    int            ifaceType,
    const QString& ifaceHost,
    const QString& ifacePort)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Scaffolding plugin…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        root  = remoteRoot,
        pname = pluginName,
        tname = targetName,
        ns    = pluginNamespace,
        desc  = description,
        tmpl  = templateType,
        itype = ifaceType,
        ihost = ifaceHost,
        iport = ifacePort] {
            const QMap<QString, QString> files =
                PluginTemplateEngine::buildFiles(pname, tname, desc, tmpl, itype, ihost, iport, ns);
            writePluginFiles(root + "/cosmos-" + pname, files);
    }));
}

void InfraViewModel::scaffoldPluginFiles(
    const QString&                remoteRoot,
    const QString&                pluginName,
    const QMap<QString, QString>& files)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Scaffolding plugin…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        root  = remoteRoot,
        pname = pluginName,
        files] {
            writePluginFiles(root + "/cosmos-" + pname, files);
    }));
}

// ── Add target to existing plugin ────────────────────────────────────────────

void InfraViewModel::addTargetToPlugin(
    const QString& pluginRoot,
    const QString& targetName,
    const QString& pluginNamespace,
    int            templateType)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Adding target…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        root  = pluginRoot,
        tname = targetName,
        ns    = pluginNamespace,
        tmpl  = templateType] {

            const QMap<QString, QString> allFiles =
                PluginTemplateEngine::buildTargetFiles(tname, tmpl, ns);

            int created = 0;
            QStringList failed;

            for (auto it = allFiles.cbegin(); it != allFiles.cend(); ++it) {
                const std::string fullPath = (root + "/" + it.key()).toStdString();
                const std::string dirPath  =
                    fullPath.substr(0, fullPath.rfind('/'));
                [[maybe_unused]] const std::string mkdirOut =
                    fs_.executeCommand("mkdir -p " + Core::Connection::shellQuote(dirPath));

                if (fs_.writeFile(fullPath, it.value().toStdString()))
                    ++created;
                else
                    failed << it.key();
            }

            const bool    ok     = failed.isEmpty();
            const QString detail = ok
                ? QString("Target %1: %2 file(s) created").arg(tname).arg(created)
                : QString("Failed: ") + failed.join(", ");

            QMetaObject::invokeMethod(this, [this, tname, ok, detail] {
                setBusy(false);
                setStatus(detail);
                emit targetAdded(tname, ok, detail);
            }, Qt::QueuedConnection);
    }));
}

// ── Add script to existing target ────────────────────────────────────────────

void InfraViewModel::addScriptToPlugin(
    const QString& pluginRoot,
    const QString& targetName,
    const QString& scriptName)
{
    if (!connected_) { setStatus("Not connected"); return; }
    setBusy(true);
    setStatus("Adding script…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        root  = pluginRoot,
        tname = targetName,
        sname = scriptName] {

            const QMap<QString, QString> allFiles =
                PluginTemplateEngine::buildScriptFile(tname, sname);

            int created = 0;
            QStringList failed;

            for (auto it = allFiles.cbegin(); it != allFiles.cend(); ++it) {
                const std::string fullPath = (root + "/" + it.key()).toStdString();
                const std::string dirPath  =
                    fullPath.substr(0, fullPath.rfind('/'));
                [[maybe_unused]] const std::string mkdirOut =
                    fs_.executeCommand("mkdir -p " + Core::Connection::shellQuote(dirPath));

                if (fs_.writeFile(fullPath, it.value().toStdString()))
                    ++created;
                else
                    failed << it.key();
            }

            const bool    ok     = failed.isEmpty();
            const QString detail = ok
                ? QString("Script %1: %2 file(s) created").arg(sname).arg(created)
                : QString("Failed: ") + failed.join(", ");

            QMetaObject::invokeMethod(this, [this, sname, ok, detail] {
                setBusy(false);
                setStatus(detail);
                emit scriptAdded(sname, ok, detail);
            }, Qt::QueuedConnection);
    }));
}

} // namespace OpenC3::ViewModels
