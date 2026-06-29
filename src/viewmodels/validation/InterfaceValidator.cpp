#include "InterfaceValidator.h"
#include "PluginKeywords.h"
#include "ProtocolRuleSupport.h"
#include "TextTokenizer.h"

#include <QSet>
#include <QStringList>

namespace OpenC3::ViewModels::Validation {

namespace {

const QSet<QString>& mapKeywords()
{
    static const QSet<QString> kMap = {
        "MAP_TARGET", "MAP_CMD_TARGET", "MAP_TLM_TARGET",
    };
    return kMap;
}

} // namespace

bool InterfaceValidator::appliesTo(const QString& /*path*/, const QString& content) const
{
    const QStringList lines = content.split('\n');
    for (const QString& raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        const QStringList toks = tokenizeConfigLine(trimmed);
        if (toks.isEmpty())
            continue;
        const QString kw = toks[0].toUpper();
        if (kw == QStringLiteral("INTERFACE") || kw == QStringLiteral("ROUTER"))
            return true;
    }
    return false;
}

ValidationReport InterfaceValidator::validate(const QString& content) const
{
    ValidationReport report;
    const QStringList lines = content.split('\n');

    bool inContext   = false;  // inside an INTERFACE / ROUTER block
    int  ifaceCount  = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = tokenizeConfigLine(trimmed);
        if (toks.isEmpty())
            continue;

        const QString kw = toks[0].toUpper();

        if (kw == QStringLiteral("INTERFACE") || kw == QStringLiteral("ROUTER")) {
            ++ifaceCount;
            inContext = true;
            // INTERFACE <name> <class_file> <params...>
            if (toks.size() < 3) {
                report.add(Diagnostic::error(
                    lineNo,
                    QStringLiteral("%1 requires <name> <class_file> ...").arg(kw),
                    QStringLiteral("interface.args"),
                    QStringLiteral("Example: INTERFACE INST_INT tcpip_client_interface.rb "
                                   "host 8080 8081 10.0 nil BURST")));
            }
            continue;
        }

        // Any other top-level declaration closes the interface context.
        if (PluginKeywords::topLevel().contains(kw)) {
            inContext = false;
            continue;
        }

        // PROTOCOL lines are validated by the shared protocol rule below.
        if (kw == QStringLiteral("PROTOCOL"))
            continue;

        if (mapKeywords().contains(kw)) {
            if (!inContext) {
                report.add(Diagnostic::warning(
                    lineNo,
                    QStringLiteral("%1 must appear inside an INTERFACE or ROUTER").arg(kw),
                    QStringLiteral("interface.maptarget.context")));
            } else if (toks.size() < 2) {
                report.add(Diagnostic::error(
                    lineNo, QStringLiteral("%1 requires a target name").arg(kw),
                    QStringLiteral("interface.maptarget.args")));
            }
            continue;
        }

        // Other modifiers: only judged when inside an interface/router (modifiers
        // belonging to TARGET/MICROSERVICE/etc. are out of scope here).
        if (inContext && !PluginKeywords::interfaceModifiers().contains(kw)) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("'%1' is not a recognised INTERFACE/ROUTER modifier")
                    .arg(toks[0]),
                QStringLiteral("interface.modifier.unknown")));
        }
    }

    // Shared deep PROTOCOL checks.
    checkProtocolLines(content, report);

    if (ifaceCount == 0) {
        report.add(Diagnostic::info(
            0, QStringLiteral("No INTERFACE or ROUTER found"),
            QStringLiteral("interface.none")));
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
