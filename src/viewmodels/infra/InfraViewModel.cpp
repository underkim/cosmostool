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
    if (!connected_) { setStatus("연결되지 않음"); return; }
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
                setStatus(ok ? "로드 완료: " + p : "파일 없음: " + p);
                if (ok) emit envLoaded(p, txt);
            }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::saveEnvFile(const QString& remotePath, const QString& content)
{
    if (!connected_) { setStatus("연결되지 않음"); return; }
    setBusy(true);
    setStatus("저장 중 …");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        path = remotePath.toStdString(),
        data = content.toStdString()] {
            const bool ok = fs_.writeFile(path, data);
            QMetaObject::invokeMethod(this, [this, p = QString::fromStdString(path), ok] {
                setBusy(false);
                setStatus(ok ? "저장 완료: " + p : "저장 실패: " + p);
                emit fileSaved(p, ok);
            }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::loadComposeFile(const QString& remotePath)
{
    if (!connected_) { setStatus("연결되지 않음"); return; }
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
                setStatus(ok ? "로드 완료: " + p : "파일 없음: " + p);
                if (ok) emit composeLoaded(p, txt);
            }, Qt::QueuedConnection);
    }));
}

void InfraViewModel::saveComposeFile(const QString& remotePath, const QString& content)
{
    if (!connected_) { setStatus("연결되지 않음"); return; }
    setBusy(true);

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        path = remotePath.toStdString(),
        data = content.toStdString()] {
            const bool ok = fs_.writeFile(path, data);
            QMetaObject::invokeMethod(this, [this, p = QString::fromStdString(path), ok] {
                setBusy(false);
                setStatus(ok ? "저장 완료: " + p : "저장 실패: " + p);
                emit fileSaved(p, ok);
            }, Qt::QueuedConnection);
    }));
}

// ── Volume override workflow ──────────────────────────────────────────────────

void InfraViewModel::loadContainers()
{
    if (!connected_) { setStatus("연결되지 않음"); return; }

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
    if (!connected_) { setStatus("연결되지 않음"); return; }
    setBusy(true);
    setStatus("컨테이너 파일 추출 중…");

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
                    setStatus(ok ? "파일 추출 완료" : "파일을 찾을 수 없습니다");
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
    if (!connected_) { setStatus("연결되지 않음"); return; }
    setBusy(true);
    setStatus("볼륨 오버라이드 파일 저장 중…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        ctr      = container.toStdString(),
        ctrPath  = containerFilePath.toStdString(),
        hostPath = hostSavePath.toStdString(),
        data     = content.toStdString()] {

            const std::string dirPath = hostPath.substr(0, hostPath.rfind('/'));
            [[maybe_unused]] const std::string mkdirOut =
                fs_.executeCommand("mkdir -p " + Core::Connection::shellQuote(dirPath));

            const bool ok = fs_.writeFile(hostPath, data);

            // compose.yaml volume entry: uses 6-space indent (service.volumes list item)
            const QString entry = QString("      - %1:%2")
                .arg(QString::fromStdString(hostPath))
                .arg(QString::fromStdString(ctrPath));
            // Helpful restart hint
            const QString hint = QString(
                "# 컨테이너: %1\n"
                "# 아래 항목을 compose.yaml의 해당 서비스 volumes: 섹션에 추가하세요:\n"
                "%2\n\n"
                "# 적용: docker compose up -d %1")
                .arg(QString::fromStdString(ctr))
                .arg(entry);

            QMetaObject::invokeMethod(this, [this, ok,
                hp   = QString::fromStdString(hostPath),
                hint] {
                    setBusy(false);
                    setStatus(ok ? "저장 완료 — compose.yaml에 볼륨 항목을 추가하세요"
                                 : "파일 저장 실패: " + hp);
                    if (ok) emit volumeEntryReady(hint);
                    emit overrideApplied(ok, hp);
                }, Qt::QueuedConnection);
    }));
}

// ── Plugin scaffolding ────────────────────────────────────────────────────────

void InfraViewModel::scaffoldPlugin(
    const QString& remoteRoot,
    const QString& pluginName,
    const QString& targetName,
    const QString& pluginNamespace,
    const QString& description,
    int            templateType)
{
    if (!connected_) { setStatus("연결되지 않음"); return; }
    setBusy(true);
    setStatus("플러그인 스캐폴딩 생성 중…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        root  = remoteRoot,
        pname = pluginName,
        tname = targetName,
        ns    = pluginNamespace,
        desc  = description,
        tmpl  = templateType] {

            const QMap<QString, QString> files =
                PluginTemplateEngine::buildFiles(pname, tname, desc, tmpl);

            const QString pluginDir = root + "/cosmos-" + pname;
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
                ? QString("%1개 파일 생성 완료 (%2)")
                      .arg(created).arg(pluginDir)
                : QString("실패: ") + failed.join(", ");

            QMetaObject::invokeMethod(this, [this, pluginDir, ok, detail] {
                setBusy(false);
                setStatus(detail);
                emit scaffoldComplete(pluginDir, ok, detail);
            }, Qt::QueuedConnection);
    }));
}

// ── Add target to existing plugin ────────────────────────────────────────────

void InfraViewModel::addTargetToPlugin(
    const QString& pluginRoot,
    const QString& targetName,
    const QString& pluginNamespace,
    int            templateType)
{
    if (!connected_) { setStatus("연결되지 않음"); return; }
    setBusy(true);
    setStatus("타겟 추가 중…");

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        root  = pluginRoot,
        tname = targetName,
        ns    = pluginNamespace,
        tmpl  = templateType] {

            const QMap<QString, QString> allFiles =
                PluginTemplateEngine::buildTargetFiles(tname, tmpl);

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
                ? QString("타겟 %1: %2개 파일 생성").arg(tname).arg(created)
                : QString("실패: ") + failed.join(", ");

            QMetaObject::invokeMethod(this, [this, tname, ok, detail] {
                setBusy(false);
                setStatus(detail);
                emit targetAdded(tname, ok, detail);
            }, Qt::QueuedConnection);
    }));
}

} // namespace OpenC3::ViewModels
