#include "ConfigValidator.h"
#include "ScreenParser.h"
#include "PluginConfigParser.h"
#include "PluginValidator.h"
#include "TargetConfigValidator.h"
#include "CmdTlmRuleSupport.h"
#include "TextTokenizer.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

namespace OpenC3::ViewModels::Validation {

namespace {

// Read a whole text file; returns false if it cannot be opened.
bool readTextFile(const QString& path, QString& out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    out = stream.readAll();
    return true;
}

// First non-comment, non-empty token of a config file (upper-cased).
QString firstKeyword(const QString& content)
{
    const QStringList lines = content.split('\n');
    for (const QString& raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        const QStringList toks = tokenizeConfigLine(trimmed);
        if (!toks.isEmpty())
            return toks[0].toUpper();
    }
    return {};
}

} // namespace

ConfigValidator::FileKind ConfigValidator::classify(const QString& path,
                                                    const QString& content)
{
    const QString base = QFileInfo(path).fileName().toLower();
    if (base == QStringLiteral("plugin.txt"))
        return FileKind::PluginConfig;
    if (base == QStringLiteral("target.txt"))
        return FileKind::TargetConfig;

    const QString kw = firstKeyword(content);
    if (kw == QStringLiteral("SCREEN"))
        return FileKind::Screen;
    if (kw == QStringLiteral("COMMAND") || kw == QStringLiteral("TELEMETRY"))
        return FileKind::CmdTlm;

    // Fall back to directory hints used by the standard COSMOS layout.
    const QString lowered = path.toLower();
    if (lowered.contains(QStringLiteral("/screens/")) ||
        lowered.contains(QStringLiteral("\\screens\\")))
        return FileKind::Screen;
    if (lowered.contains(QStringLiteral("/cmd_tlm/")) ||
        lowered.contains(QStringLiteral("\\cmd_tlm\\")))
        return FileKind::CmdTlm;

    return FileKind::Unknown;
}

ValidationReport ConfigValidator::validateContent(FileKind kind, const QString& content)
{
    switch (kind) {
    case FileKind::CmdTlm:
        // CMD and TLM share one engine; the file-level path runs the union of
        // both rule sets (std::nullopt = keep every scope).
        return validateCmdTlmScoped(content, std::nullopt);
    case FileKind::Screen:
        return ScreenParser::parse(content);
    case FileKind::PluginConfig:
        // PluginValidator merges structural plugin.txt checks with the deep
        // PROTOCOL checks (single source of protocol truth).
        return PluginValidator{}.validate(content);
    case FileKind::TargetConfig:
        return TargetConfigValidator{}.validate(content);
    case FileKind::Unknown:
        break;
    }
    return {};
}

ValidationReport ConfigValidator::validateFile(const QString& path)
{
    ValidationReport report;

    QString content;
    if (!readTextFile(path, content)) {
        report.add(Diagnostic::error(
            0, QStringLiteral("Unable to open file"), QStringLiteral("io.open")));
        report.setFilePath(path);
        return report;
    }

    const FileKind kind = classify(path, content);
    if (kind == FileKind::Unknown) {
        report.add(Diagnostic::info(
            0, QStringLiteral("Not a recognised COSMOS config file; skipped"),
            QStringLiteral("classify.unknown")));
        report.setFilePath(path);
        return report;
    }

    report = validateContent(kind, content);
    report.setFilePath(path);
    return report;
}

ValidationReport ConfigValidator::validateFolder(const QString& dir)
{
    ValidationReport report;

    QDirIterator it(dir, QStringList{ QStringLiteral("*.txt") },
                    QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString path = it.next();

        QString content;
        if (!readTextFile(path, content))
            continue;

        const FileKind kind = classify(path, content);
        if (kind == FileKind::Unknown)
            continue;

        ValidationReport one = validateContent(kind, content);
        one.setFilePath(path);
        report.merge(one);

        // Cross-file check: TARGET folders referenced by a plugin.txt must
        // exist under <plugin_dir>/targets/ when that standard layout is used.
        if (kind == FileKind::PluginConfig) {
            const auto parsed     = PluginConfigParser::parse(content);
            const QString pluginDir = QFileInfo(path).absolutePath();
            const QDir targetsDir(pluginDir + QStringLiteral("/targets"));
            if (targetsDir.exists()) {
                for (int i = 0; i < parsed.targetFolders.size(); ++i) {
                    const QString folder = parsed.targetFolders[i];
                    const int     line   = (i < parsed.targetFolderLines.size())
                                               ? parsed.targetFolderLines[i] : 0;
                    if (!QDir(targetsDir.filePath(folder)).exists()) {
                        Diagnostic d = Diagnostic::error(
                            line,
                            QStringLiteral("TARGET folder 'targets/%1' was not found").arg(folder),
                            QStringLiteral("plugin.target.folder"),
                            QStringLiteral("Create the targets/%1 directory or fix the TARGET name.")
                                .arg(folder));
                        d.filePath = path;
                        report.add(d);
                    }
                }
            }
        }
    }

    if (report.diagnostics.isEmpty())
        report.add(Diagnostic::info(
            0, QStringLiteral("No COSMOS config files found under this folder"),
            QStringLiteral("folder.empty")));

    return report;
}

QString ConfigValidator::fileKindLabel(FileKind kind)
{
    switch (kind) {
    case FileKind::CmdTlm:       return QStringLiteral("CMD/TLM");
    case FileKind::Screen:       return QStringLiteral("Screen");
    case FileKind::PluginConfig: return QStringLiteral("Plugin");
    case FileKind::TargetConfig: return QStringLiteral("Target");
    case FileKind::Unknown:      return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

} // namespace OpenC3::ViewModels::Validation
