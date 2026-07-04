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
    ///
    /// ifaceType: 0=TCP/IP Client, 1=TCP/IP Server, 2=UDP, 3=Serial.
    /// Pass -1 (default) to derive it from templateType instead, matching
    /// this function's original (pre-interface-picker) behavior.
    [[nodiscard]] static QMap<QString, QString> buildFiles(
        const QString& pluginName,
        const QString& targetName,
        const QString& description,
        int            templateType,
        int            ifaceType = -1,
        const QString& ifaceHost = QStringLiteral("localhost"),
        const QString& ifacePort = QStringLiteral("8080"),
        const QString& pluginNamespace = QString());

    /// Returns only the files that live under  targets/<TARGET>/
    /// Used when adding a target to an already-existing plugin.
    [[nodiscard]] static QMap<QString, QString> buildTargetFiles(
        const QString& targetName,
        int            templateType,
        const QString& pluginNamespace = QString());
};

} // namespace OpenC3::ViewModels
