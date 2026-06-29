#include "ConfigValidator.h"

#include "PluginConfigParser.h"
#include "TargetConfigParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace OpenC3::ViewModels::Validation {

FileKind ConfigValidator::classify(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (info.fileName().compare(QStringLiteral("plugin.txt"), Qt::CaseInsensitive) == 0) {
        return FileKind::PluginConfig;
    }
    if (info.fileName().compare(QStringLiteral("target.txt"), Qt::CaseInsensitive) == 0) {
        return FileKind::TargetConfig;
    }
    return FileKind::Unknown;
}

ValidationReport ConfigValidator::validateContent(const QString& content, const QString& filePath)
{
    switch (classify(filePath)) {
    case FileKind::PluginConfig:
        return PluginConfigParser::validate(content, filePath);
    case FileKind::TargetConfig:
        return TargetConfigParser::validate(content, filePath);
    case FileKind::Unknown:
        break;
    }

    ValidationReport report;
    report.diagnostics.append({
        filePath,
        0,
        Severity::Warning,
        QStringLiteral("No validator is registered for this file type."),
        QStringLiteral("Add a ConfigValidator classifier entry before expecting diagnostics for this file."),
        QStringLiteral("config.filekind.unknown")
    });
    return report;
}

ValidationReport ConfigValidator::validateFolder(const QString& folderPath)
{
    ValidationReport report;
    QSet<QString> targetFolders;
    bool sawPlugin = false;

    QDirIterator it(folderPath, QStringList() << QStringLiteral("*.txt"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        const FileKind kind = classify(filePath);
        if (kind == FileKind::Unknown) continue;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            report.diagnostics.append({
                filePath,
                0,
                Severity::Error,
                QStringLiteral("Unable to read validation input file."),
                QStringLiteral("Check file permissions and whether the path exists."),
                QStringLiteral("config.file.read")
            });
            continue;
        }

        ValidationReport fileReport = validateContent(QString::fromUtf8(file.readAll()), filePath);
        report.diagnostics += fileReport.diagnostics;

        if (kind == FileKind::PluginConfig) {
            sawPlugin = true;
        } else if (kind == FileKind::TargetConfig) {
            targetFolders.insert(QFileInfo(filePath).absoluteDir().dirName().toUpper());
        }
    }

    if (!sawPlugin) {
        report.diagnostics.append({
            folderPath,
            0,
            Severity::Warning,
            QStringLiteral("No plugin.txt file was found in the selected folder."),
            QStringLiteral("Validate from the plugin root so plugin and target references can be checked together."),
            QStringLiteral("config.folder.plugin_missing")
        });
    }

    if (targetFolders.isEmpty()) {
        report.diagnostics.append({
            folderPath,
            0,
            Severity::Info,
            QStringLiteral("No target.txt files were found under targets/."),
            QStringLiteral("Add target.txt files when the plugin defines COSMOS targets."),
            QStringLiteral("config.folder.targets_missing")
        });
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
