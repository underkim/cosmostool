#pragma once

#include <QMap>
#include <QString>

namespace OpenC3::ViewModels {

/// Generates the source file tree for a new OpenC3 COSMOS plugin or target.
///
/// This is pure template expansion — no Qt signals, no remote I/O.
/// Callers that need to write files to a remote host must pass the returned
/// map to IRemoteFileService themselves.
///
/// Template types:
///   0 = Generic  (simple ping/status packets)
///   1 = CCSDS    (CCSDS primary/secondary header layout)
///   2 = GSE      (ground support equipment, TCP/IP)
class PluginTemplateEngine {
public:
    /// Returns a map of  { relativePath → fileContent }  relative to the
    /// plugin root directory.  The caller decides where to write the files.
    [[nodiscard]] static QMap<QString, QString> buildFiles(
        const QString& pluginName,
        const QString& targetName,
        const QString& description,
        int            templateType);

    /// Returns only the files that live under  targets/<TARGET>/
    /// Used when adding a target to an already-existing plugin.
    [[nodiscard]] static QMap<QString, QString> buildTargetFiles(
        const QString& targetName,
        int            templateType);
};

} // namespace OpenC3::ViewModels
