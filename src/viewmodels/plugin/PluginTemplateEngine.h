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

    /// The "<class_file.rb> <args...>" portion of an INTERFACE/ROUTER line
    /// for the given connection type (0=TCP/IP Client, 1=TCP/IP Server,
    /// 2=UDP, 3=Serial) - the same mapping buildFiles() uses internally,
    /// exposed so other guided-creation UI (e.g. the Manifest tab's "New
    /// Block" dialog) can generate an identical, correct line without
    /// duplicating COSMOS's interface class/argument conventions.
    [[nodiscard]] static QString buildInterfaceArgs(
        int ifaceType, const QString& host, const QString& port);

    /// Returns a single-file map  { "targets/<TARGET>/procedures/<name>.rb" →
    /// content }  - a beginner-oriented script boilerplate (cmd/wait_check
    /// pair wrapped in a method, matching the same style as the checkout
    /// script buildTargetFiles() already generates) for the Manifest tab's
    /// "Add Script" guided creation flow.
    [[nodiscard]] static QMap<QString, QString> buildScriptFile(
        const QString& targetName, const QString& scriptName);
};

} // namespace OpenC3::ViewModels
