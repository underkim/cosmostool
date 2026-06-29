#include "PluginConfigParser.h"
#include "TextTokenizer.h"

#include <QSet>
#include <QVector>

namespace OpenC3::ViewModels::Validation {

namespace {

// Top-level declarations that open a context modifiers attach to.
const QSet<QString>& topLevelKeywords()
{
    static const QSet<QString> kTop = {
        "VARIABLE", "NEEDS_DEPENDENCIES",
        "TARGET", "INTERFACE", "ROUTER", "MICROSERVICE",
        "TOOL", "WIDGET", "SCRIPT_ENGINE",
    };
    return kTop;
}

// Modifiers that may appear under INTERFACE / ROUTER.
const QSet<QString>& interfaceModifiers()
{
    static const QSet<QString> kMods = {
        "MAP_TARGET", "MAP_CMD_TARGET", "MAP_TLM_TARGET",
        "PROTOCOL", "OPTION", "SECRET", "ENV", "WORK_DIR",
        "DONT_CONNECT", "DONT_RECONNECT", "RECONNECT_DELAY",
        "DISABLE_DISCONNECT", "LOG_RAW", "PORT", "CMD",
        "CONTAINER", "ROUTE_PREFIX", "SHUTDOWN_DELAY",
        "CONNECT_ON_STARTUP", "AUTO_RECONNECT",
    };
    return kMods;
}

// Modifiers that may appear under MICROSERVICE.
const QSet<QString>& microserviceModifiers()
{
    static const QSet<QString> kMods = {
        "ENV", "WORK_DIR", "CMD", "OPTION", "TOPIC", "TARGET_NAME",
        "SECRET", "PORT", "CONTAINER", "ROUTE_PREFIX", "DISABLE_ERB",
        "SHARD", "STOPPED",
    };
    return kMods;
}

// Modifiers that may appear under TARGET.
const QSet<QString>& targetModifiers()
{
    static const QSet<QString> kMods = {
        "CMD_BUFFER_DEPTH", "CMD_LOG_CYCLE_TIME", "CMD_LOG_CYCLE_SIZE",
        "CMD_LOG_RETAIN_TIME", "CMD_DECOM_LOG_CYCLE_TIME",
        "CMD_DECOM_LOG_CYCLE_SIZE", "CMD_DECOM_LOG_RETAIN_TIME",
        "TLM_BUFFER_DEPTH", "TLM_LOG_CYCLE_TIME", "TLM_LOG_CYCLE_SIZE",
        "TLM_LOG_RETAIN_TIME", "TLM_DECOM_LOG_CYCLE_TIME",
        "TLM_DECOM_LOG_CYCLE_SIZE", "TLM_DECOM_LOG_RETAIN_TIME",
        "REDUCED_MINUTE_LOG_RETAIN_TIME", "REDUCED_HOUR_LOG_RETAIN_TIME",
        "REDUCED_DAY_LOG_RETAIN_TIME", "LOG_RETAIN_TIME",
        "REDUCER_DISABLE", "REDUCER_MAX_CPU_UTILIZATION",
        "TARGET_MICROSERVICE", "PACKET", "DISABLE_ERB", "SHARD",
    };
    return kMods;
}

// Modifiers that may appear under TOOL / WIDGET.
const QSet<QString>& toolModifiers()
{
    static const QSet<QString> kMods = {
        "URL", "INLINE_URL", "ICON", "CATEGORY", "SHOWN",
        "POSITION", "WINDOW", "DISABLE_ERB", "IMPORT_MAP_ITEM",
    };
    return kMods;
}

enum class Context { None, Interface, Router, Microservice, Target, Tool, Variable };

bool modifierAllowed(Context ctx, const QString& kw)
{
    switch (ctx) {
    case Context::Interface:
    case Context::Router:
        return interfaceModifiers().contains(kw);
    case Context::Microservice:
        return microserviceModifiers().contains(kw);
    case Context::Target:
        return targetModifiers().contains(kw);
    case Context::Tool:
        return toolModifiers().contains(kw);
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

        // ── PROTOCOL (special-cased for direction + context) ─────────────────
        if (kw == "PROTOCOL") {
            if (ctx != Context::Interface && ctx != Context::Router) {
                report.add(Diagnostic::error(
                    lineNo,
                    QStringLiteral("PROTOCOL must appear inside an INTERFACE or ROUTER"),
                    QStringLiteral("plugin.protocol.context"),
                    QStringLiteral("Place PROTOCOL after an INTERFACE/ROUTER declaration.")));
                continue;
            }
            if (toks.size() < 3) {
                report.add(Diagnostic::error(
                    lineNo,
                    QStringLiteral("PROTOCOL requires <READ|WRITE|READ_WRITE> <protocol_class> ..."),
                    QStringLiteral("plugin.protocol.args"),
                    QStringLiteral("Example: PROTOCOL READ_WRITE length_protocol.rb 0 16 0")));
                continue;
            }
            const QString dir = toks[1].toUpper();
            if (dir != "READ" && dir != "WRITE" && dir != "READ_WRITE") {
                report.add(Diagnostic::error(
                    lineNo,
                    QStringLiteral("PROTOCOL direction '%1' must be READ, WRITE, or READ_WRITE")
                        .arg(toks[1]),
                    QStringLiteral("plugin.protocol.direction")));
            }
            continue;
        }

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
