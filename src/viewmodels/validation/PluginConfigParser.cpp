#include "PluginConfigParser.h"
#include "PluginKeywords.h"
#include "TextTokenizer.h"

#include <QSet>
#include <QVector>

namespace OpenC3::ViewModels::Validation {

namespace {

// Recognised keyword sets are shared with InterfaceValidator (PluginKeywords).
const QSet<QString>& topLevelKeywords() { return PluginKeywords::topLevel(); }

enum class Context { None, Interface, Router, Microservice, Target, Tool, Variable };

bool modifierAllowed(Context ctx, const QString& kw)
{
    switch (ctx) {
    case Context::Interface:
    case Context::Router:
        return PluginKeywords::interfaceModifiers().contains(kw);
    case Context::Microservice:
        return PluginKeywords::microserviceModifiers().contains(kw);
    case Context::Target:
        return PluginKeywords::targetModifiers().contains(kw);
    case Context::Tool:
        return PluginKeywords::toolModifiers().contains(kw);
    case Context::None:
    case Context::Variable:
        return false;
    }
    return false;
}

} // namespace

PluginConfigParser::Result PluginConfigParser::parse(const QString& content)
{
    Result result;
    ValidationReport& report = result.report;

    const QStringList lines = content.split('\n');

    Context ctx = Context::None;

    struct TargetRef { QString name; int line{0}; };
    QVector<TargetRef> mappedTargets; // MAP_TARGET references to resolve later

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = tokenizeConfigLine(trimmed);
        if (toks.isEmpty())
            continue;

        const QString kw = toks[0].toUpper();

        // ── Top-level declarations ───────────────────────────────────────────
        if (topLevelKeywords().contains(kw)) {
            if (kw == "TARGET") {
                // TARGET <folder_name> <target_name>
                if (toks.size() < 3) {
                    report.add(Diagnostic::error(
                        lineNo, QStringLiteral("TARGET requires <folder> <name>"),
                        QStringLiteral("plugin.target.args"),
                        QStringLiteral("Example: TARGET INST INST")));
                } else {
                    result.targetFolders     << toks[1];
                    result.targetFolderLines << lineNo;
                    result.declaredTargets   << toks[2].toUpper();
                }
                ctx = Context::Target;
            } else if (kw == "INTERFACE" || kw == "ROUTER") {
                // INTERFACE <name> <class_file> <params...>
                if (toks.size() < 3) {
                    report.add(Diagnostic::error(
                        lineNo,
                        QStringLiteral("%1 requires <name> <class_file> ...").arg(kw),
                        QStringLiteral("plugin.interface.args"),
                        QStringLiteral("Example: INTERFACE INST_INT tcpip_client_interface.rb host 8080 8081 10.0 nil BURST")));
                } else {
                    result.declaredInterfaces << toks[1].toUpper();
                }
                ctx = (kw == "INTERFACE") ? Context::Interface : Context::Router;
            } else if (kw == "MICROSERVICE") {
                // MICROSERVICE <folder> <name>
                if (toks.size() < 3) {
                    report.add(Diagnostic::error(
                        lineNo, QStringLiteral("MICROSERVICE requires <folder> <name>"),
                        QStringLiteral("plugin.microservice.args")));
                }
                ctx = Context::Microservice;
            } else if (kw == "TOOL" || kw == "WIDGET") {
                ctx = Context::Tool;
            } else if (kw == "VARIABLE") {
                // VARIABLE <name> <default_value>
                if (toks.size() < 3) {
                    report.add(Diagnostic::warning(
                        lineNo, QStringLiteral("VARIABLE should have a name and default value"),
                        QStringLiteral("plugin.variable.args"),
                        QStringLiteral("Example: VARIABLE port 8080")));
                }
                ctx = Context::Variable;
            } else {
                // NEEDS_DEPENDENCIES / SCRIPT_ENGINE — no nested context
                ctx = Context::None;
            }
            continue;
        }

        // ── PROTOCOL is validated by the shared protocol rule ────────────────
        // (ProtocolRuleSupport / ProtocolValidator). Skip it here so plugin.txt
        // gets exactly one set of protocol diagnostics, not two.
        if (kw == "PROTOCOL")
            continue;

        // ── MAP_TARGET reference (resolved after the full parse) ──────────────
        if (kw == "MAP_TARGET" || kw == "MAP_CMD_TARGET" || kw == "MAP_TLM_TARGET") {
            if (ctx != Context::Interface && ctx != Context::Router) {
                report.add(Diagnostic::error(
                    lineNo,
                    QStringLiteral("%1 must appear inside an INTERFACE or ROUTER").arg(kw),
                    QStringLiteral("plugin.maptarget.context")));
            } else if (toks.size() < 2) {
                report.add(Diagnostic::error(
                    lineNo, QStringLiteral("%1 requires a target name").arg(kw),
                    QStringLiteral("plugin.maptarget.args")));
            } else {
                mappedTargets.append(TargetRef{ toks[1].toUpper(), lineNo });
            }
            continue;
        }

        // ── Other modifiers ──────────────────────────────────────────────────
        if (ctx == Context::None) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("'%1' is not inside a TARGET/INTERFACE/ROUTER/MICROSERVICE block")
                    .arg(toks[0]),
                QStringLiteral("plugin.modifier.orphan")));
            continue;
        }

        if (!modifierAllowed(ctx, kw)) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("'%1' is not a recognised modifier in this block").arg(toks[0]),
                QStringLiteral("plugin.modifier.unknown")));
        }
    }

    // ── Resolve MAP_TARGET references against declared targets ───────────────
    const QSet<QString> declared(result.declaredTargets.begin(),
                                 result.declaredTargets.end());
    for (const auto& ref : mappedTargets) {
        if (!declared.contains(ref.name)) {
            report.add(Diagnostic::warning(
                ref.line,
                QStringLiteral("MAP_TARGET references target '%1' which is not declared in this plugin")
                    .arg(ref.name),
                QStringLiteral("plugin.maptarget.undeclared"),
                QStringLiteral("Add a matching TARGET declaration or check the name.")));
        }
    }

    return result;
}

} // namespace OpenC3::ViewModels::Validation
