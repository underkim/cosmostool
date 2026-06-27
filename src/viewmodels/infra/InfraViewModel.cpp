#include "InfraViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QDateTime>
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

// ── Private helpers ───────────────────────────────────────────────────────────

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

// ── Patch generation ──────────────────────────────────────────────────────────

void InfraViewModel::generatePatch(
    const QString& originalContent,
    const QString& currentContent,
    const QString& filename)
{
    if (originalContent == currentContent) {
        emit patchReady("(변경 없음)");
        return;
    }

    // Run on background thread — writing temp files + git diff can block briefly.
    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

    watcher->setFuture(QtConcurrent::run([this,
        orig = originalContent,
        curr = currentContent,
        fname = filename] {
            const QString patch = computePatch(
                orig, curr,
                "a/" + fname,
                "b/" + fname);
            QMetaObject::invokeMethod(this, [this, patch] {
                emit patchReady(patch);
            }, Qt::QueuedConnection);
    }));
}

// static
QString InfraViewModel::computePatch(
    const QString& original,
    const QString& modified,
    const QString& filenameA,
    const QString& filenameB)
{
    // Preferred: git diff --no-index writes a proper unified diff.
    const QString tmpA = QDir::tempPath() + "/cosmos_patch_orig.tmp";
    const QString tmpB = QDir::tempPath() + "/cosmos_patch_curr.tmp";

    auto writeTmp = [](const QString& path, const QString& content) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(content.toUtf8());
    };
    writeTmp(tmpA, original);
    writeTmp(tmpB, modified);

    QProcess git;
    git.start("git", {"diff", "--no-index",
                       "--src-prefix=" + filenameA.section('/', 0, 0) + "/",
                       "--dst-prefix=" + filenameB.section('/', 0, 0) + "/",
                       tmpA, tmpB});
    git.waitForFinished(8000);
    QString patch = QString::fromUtf8(git.readAllStandardOutput());

    QFile::remove(tmpA);
    QFile::remove(tmpB);

    if (!patch.isEmpty()) {
        // Replace temp paths with meaningful names
        patch.replace(tmpA, filenameA);
        patch.replace(tmpB, filenameB);
        return patch;
    }

    // Fallback: simple line-by-line unified diff
    const QStringList lA = original.split('\n');
    const QStringList lB = modified.split('\n');

    QString result;
    result += "--- " + filenameA + "\n";
    result += "+++ " + filenameB + "\n";

    // Emit hunks using a basic linear scan (no LCS — good enough for config files)
    int i = 0, j = 0;
    while (i < lA.size() || j < lB.size()) {
        if (i < lA.size() && j < lB.size() && lA[i] == lB[j]) {
            ++i; ++j;
            continue;
        }
        // Start a hunk
        const int startA = i, startB = j;
        QStringList removed, added;
        while (i < lA.size() && (j >= lB.size() || lA[i] != lB[j])) {
            removed << lA[i++];
        }
        while (j < lB.size() && (i >= lA.size() || lA[i] != lB[j])) {
            added << lB[j++];
        }
        if (removed.isEmpty() && added.isEmpty()) { ++i; ++j; continue; }

        result += QString("@@ -%1,%2 +%3,%4 @@\n")
                      .arg(startA + 1).arg(removed.size())
                      .arg(startB + 1).arg(added.size());
        for (const auto& l : std::as_const(removed))  result += "-" + l + "\n";
        for (const auto& l : std::as_const(added))    result += "+" + l + "\n";
    }

    return result;
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
                buildScaffoldFiles(pname, tname, ns, desc, tmpl);

            const QString pluginDir = root + "/cosmos-" + pname;
            int created = 0;
            QStringList failed;

            for (auto it = files.cbegin(); it != files.cend(); ++it) {
                const std::string fullPath = (pluginDir + "/" + it.key()).toStdString();

                // Ensure parent directory (mkdir -p equivalent via executor)
                const std::string dirPath = fullPath.substr(0, fullPath.rfind('/'));
                (void)fs_.executeCommand("mkdir -p '" + dirPath + "'");

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

// ── Scaffold template builder ─────────────────────────────────────────────────

// static
QMap<QString, QString> InfraViewModel::buildScaffoldFiles(
    const QString& pluginName,
    const QString& targetName,
    const QString& ns,
    const QString& description,
    int            templateType)
{
    const QString tgt    = targetName.toUpper();
    const QString gem    = "cosmos-" + pluginName;
    const QString varPfx = pluginName.toLower().replace('-', '_');
    (void)ns; // used in template expansion below
    (void)templateType;

    QMap<QString, QString> files;

    // ── plugin.txt ────────────────────────────────────────────────────────────
    const QString interfaceClass = (templateType == 2)
        ? "tcpip_client_interface.rb localhost 8080 8080 10 nil BURST"
        : "tcpip_client_interface.rb localhost 8080 8080 10 nil BURST";

    files["plugin.txt"] =
        QString(
            "# OpenC3 COSMOS Plugin: %1\n"
            "# Description: %2\n\n"
            "VARIABLE %3_target_name %4\n\n"
            "TARGET %4 <%= %3_target_name %>\n"
            "INTERFACE <%= %3_target_name %>_INT %5\n"
            "  MAP_TARGET <%= %3_target_name %>\n"
        ).arg(pluginName, description, varPfx, tgt, interfaceClass);

    // ── gemspec ───────────────────────────────────────────────────────────────
    files[gem + ".gemspec"] =
        QString(
            "# encoding: ascii-8bit\n\n"
            "lib = File.expand_path('../lib', __FILE__)\n"
            "$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)\n\n"
            "Gem::Specification.new do |s|\n"
            "  s.name        = '%1'\n"
            "  s.version     = '0.0.1'\n"
            "  s.platform    = Gem::Platform::RUBY\n"
            "  s.summary     = '%2'\n"
            "  s.description = '%2'\n"
            "  s.homepage    = ''\n"
            "  s.license     = 'Nonstandard'\n\n"
            "  s.authors     = ['']\n"
            "  s.email       = ['']\n\n"
            "  s.files       = Dir['{targets,lib,spec}/**/*', 'plugin.txt']\n"
            "  s.require_paths = ['lib']\n"
            "end\n"
        ).arg(gem, description);

    // ── Gemfile ───────────────────────────────────────────────────────────────
    files["Gemfile"] =
        "# frozen_string_literal: true\n\n"
        "source 'https://rubygems.org'\n"
        "gemspec\n";

    // ── CMD/TLM definitions ───────────────────────────────────────────────────
    const QString cmdPath = "targets/" + tgt + "/cmd_tlm/" +
                            targetName.toLower() + "_cmds.txt";
    const QString tlmPath = "targets/" + tgt + "/cmd_tlm/" +
                            targetName.toLower() + "_tlm.txt";

    if (templateType == 1) {
        // Satellite target — CCSDS-style
        files[cmdPath] =
            QString(
                "# %1 Commands\n\n"
                "COMMAND %2 NOOP BIG_ENDIAN 'No operation'\n"
                "  APPEND_ID_PARAMETER CCSDS_STREAMID 16 UINT MIN MAX 0x1800 'Stream ID'\n"
                "    FORMAT_STRING '0x%%04X'\n"
                "  APPEND_PARAMETER CCSDS_SEQUENCE 16 UINT MIN MAX 0xC000 'Sequence Control'\n"
                "  APPEND_PARAMETER CCSDS_LENGTH 16 UINT MIN MAX 0 'Data Length'\n"
                "  APPEND_PARAMETER CCSDS_FUNCCODE 8 UINT MIN MAX 0 'Function Code'\n"
                "  APPEND_PARAMETER CCSDS_CHECKSUM 8 UINT MIN MAX 0 'Checksum'\n\n"
                "COMMAND %2 RESET BIG_ENDIAN 'Software Reset'\n"
                "  APPEND_ID_PARAMETER CCSDS_STREAMID 16 UINT MIN MAX 0x1801 'Stream ID'\n"
                "    FORMAT_STRING '0x%%04X'\n"
                "  APPEND_PARAMETER CCSDS_SEQUENCE 16 UINT MIN MAX 0xC000 'Sequence Control'\n"
                "  APPEND_PARAMETER CCSDS_LENGTH 16 UINT MIN MAX 0 'Data Length'\n"
                "  APPEND_PARAMETER CCSDS_FUNCCODE 8 UINT MIN MAX 1 'Function Code'\n"
                "  APPEND_PARAMETER CCSDS_CHECKSUM 8 UINT MIN MAX 0 'Checksum'\n"
            ).arg(tgt, tgt);

        files[tlmPath] =
            QString(
                "# %1 Telemetry\n\n"
                "TELEMETRY %2 HK BIG_ENDIAN 'Housekeeping'\n"
                "  APPEND_ID_ITEM CCSDS_STREAMID 16 UINT 0x0800 'Stream ID'\n"
                "    FORMAT_STRING '0x%%04X'\n"
                "  APPEND_ITEM CCSDS_SEQUENCE 16 UINT 'Sequence Control'\n"
                "  APPEND_ITEM CCSDS_LENGTH 16 UINT 'Packet Length'\n"
                "  APPEND_ITEM CCSDS_SECONDS 32 UINT 'Time Seconds'\n"
                "  APPEND_ITEM CCSDS_SUBSECONDS 16 UINT 'Time Subseconds'\n"
                "  APPEND_ITEM MODE 8 UINT 'Operating Mode'\n"
                "    STATES SAFE 0 NOMINAL 1 FAULT 2\n"
                "  APPEND_ITEM TEMP 16 INT 'Temperature (°C x 10)'\n"
                "    UNITS Celsius C\n"
                "    POLYNOMIAL_CONVERSION_FACTOR 0 0.1\n"
            ).arg(tgt, tgt);
    } else {
        // Generic target
        files[cmdPath] =
            QString(
                "# %1 Commands\n\n"
                "COMMAND %2 PING BIG_ENDIAN 'Ping command'\n"
                "  APPEND_PARAMETER CMD_ID 8 UINT 0 255 0x01 'Command ID'\n"
                "  APPEND_PARAMETER LENGTH 8 UINT 0 255 0 'Payload Length'\n\n"
                "COMMAND %2 RESET BIG_ENDIAN 'Reset command'\n"
                "  APPEND_PARAMETER CMD_ID 8 UINT 0 255 0x02 'Command ID'\n"
                "  APPEND_PARAMETER LENGTH 8 UINT 0 255 0 'Payload Length'\n"
            ).arg(tgt, tgt);

        files[tlmPath] =
            QString(
                "# %1 Telemetry\n\n"
                "TELEMETRY %2 STATUS BIG_ENDIAN 'Status packet'\n"
                "  APPEND_ITEM TLM_ID 8 UINT 'Telemetry ID'\n"
                "  APPEND_ITEM LENGTH 8 UINT 'Length'\n"
                "  APPEND_ITEM STATUS 8 UINT 'Status byte'\n"
                "    STATES IDLE 0 RUNNING 1 ERROR 2\n"
                "  APPEND_ITEM COUNTER 16 UINT 'Packet counter'\n"
            ).arg(tgt, tgt);
    }

    // ── Screen definition ─────────────────────────────────────────────────────
    const QString screenPath = "targets/" + tgt + "/screens/" +
                               targetName.toLower() + ".txt";
    files[screenPath] =
        QString(
            "SCREEN AUTO AUTO 1.0\n\n"
            "TITLE '%1 Status'\n\n"
            "VERTICALBOX\n"
            "  LABELVALUE %2 HK MODE\n"
            "  LABELVALUE %2 HK TEMP\n"
            "END\n"
        ).arg(tgt, tgt);

    // ── Procedures ────────────────────────────────────────────────────────────
    const QString procPath = "targets/" + tgt + "/procedures/" +
                             targetName.toLower() + "_check.rb";
    files[procPath] =
        QString(
            "# %1 checkout procedure\n\n"
            "load_utility 'cosmos_lib/procedures/utilities/clear'\n\n"
            "def %2_check\n"
            "  puts 'Starting %1 checkout...'\n\n"
            "  # Send NOOP / PING\n"
            "  cmd('%2 NOOP')\n"
            "  wait_check('%2 STATUS STATUS == \"RUNNING\"', 5)\n\n"
            "  puts '%1 checkout PASSED'\n"
            "rescue => e\n"
            "  puts \"FAILED: #{e.message}\"\n"
            "  raise\n"
            "end\n\n"
            "%2_check\n"
        ).arg(tgt, tgt.toLower());

    return files;
}

} // namespace OpenC3::ViewModels
