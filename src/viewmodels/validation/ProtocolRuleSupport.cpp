#include "ProtocolRuleSupport.h"
#include "TextTokenizer.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace OpenC3::ViewModels::Validation {

namespace {

// Top-level keywords that open an INTERFACE/ROUTER context PROTOCOL may live in.
const QSet<QString>& interfaceOpeners()
{
    static const QSet<QString> kOpen = { "INTERFACE", "ROUTER" };
    return kOpen;
}

// Top-level keywords that close any interface/router context.
const QSet<QString>& contextClosers()
{
    static const QSet<QString> kClose = {
        "TARGET", "MICROSERVICE", "TOOL", "WIDGET",
        "VARIABLE", "NEEDS_DEPENDENCIES", "SCRIPT_ENGINE",
    };
    return kClose;
}

// Built-in COSMOS protocol classes mapped to the number of *required* positional
// arguments (parameters that have no default in the COSMOS implementation).
// Keyed by the lower-cased file basename. Unknown classes are treated as custom.
const QHash<QString, int>& builtinProtocols()
{
    static const QHash<QString, int> kProtocols = {
        { "burst_protocol.rb",         0 },
        { "fixed_protocol.rb",         1 }, // minimum_id_size
        { "length_protocol.rb",        0 },
        { "terminated_protocol.rb",    2 }, // write + read termination chars
        { "template_protocol.rb",      2 }, // write + read termination chars
        { "preidentified_protocol.rb", 0 },
        { "cmd_response_protocol.rb",  0 },
        { "crc_protocol.rb",           0 },
        { "ignore_packet_protocol.rb", 2 }, // target_name + packet_name
    };
    return kProtocols;
}

// Normalise a protocol class token to a "<name>_protocol.rb" basename so both
// "length_protocol.rb", "path/length_protocol.rb" and "length_protocol" match.
QString normaliseClass(const QString& raw)
{
    QString name = raw;
    const int slash = std::max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
    if (slash >= 0)
        name = name.mid(slash + 1);
    name = name.toLower();
    if (!name.endsWith(QStringLiteral(".rb")))
        name += QStringLiteral(".rb");
    return name;
}

} // namespace

int checkProtocolLines(const QString& content, ValidationReport& report)
{
    const QStringList lines = content.split('\n');

    bool inInterface  = false;
    int  protocolCount = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = tokenizeConfigLine(trimmed);
        if (toks.isEmpty())
            continue;

        const QString kw = toks[0].toUpper();

        // Track the INTERFACE / ROUTER context PROTOCOL must sit inside.
        if (interfaceOpeners().contains(kw)) {
            inInterface = true;
            continue;
        }
        if (contextClosers().contains(kw)) {
            inInterface = false;
            continue;
        }

        if (kw != QStringLiteral("PROTOCOL"))
            continue;

        ++protocolCount;

        if (!inInterface) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("PROTOCOL appears outside an INTERFACE or ROUTER block"),
                QStringLiteral("protocol.context"),
                QStringLiteral("Place PROTOCOL lines after an INTERFACE/ROUTER declaration.")));
        }

        // Direction token.
        if (toks.size() < 2) {
            report.add(Diagnostic::error(
                lineNo,
                QStringLiteral("PROTOCOL requires a direction (READ|WRITE|READ_WRITE) "
                               "and a protocol class"),
                QStringLiteral("protocol.args"),
                QStringLiteral("Example: PROTOCOL READ_WRITE length_protocol.rb 0 16 0")));
            continue;
        }
        const QString dir = toks[1].toUpper();
        if (dir != QStringLiteral("READ") && dir != QStringLiteral("WRITE")
            && dir != QStringLiteral("READ_WRITE")) {
            report.add(Diagnostic::error(
                lineNo,
                QStringLiteral("PROTOCOL direction '%1' must be READ, WRITE, or READ_WRITE")
                    .arg(toks[1]),
                QStringLiteral("protocol.direction")));
        }

        // Protocol class.
        if (toks.size() < 3) {
            report.add(Diagnostic::error(
                lineNo,
                QStringLiteral("PROTOCOL is missing its protocol class"),
                QStringLiteral("protocol.class.missing"),
                QStringLiteral("Example: PROTOCOL READ burst_protocol.rb")));
            continue;
        }

        const QString cls   = normaliseClass(toks[2]);
        const int     given = static_cast<int>(toks.size()) - 3; // args after class

        const auto it = builtinProtocols().constFind(cls);
        if (it == builtinProtocols().constEnd()) {
            report.add(Diagnostic::info(
                lineNo,
                QStringLiteral("Protocol class '%1' is not a built-in COSMOS protocol; "
                               "arguments not checked").arg(toks[2]),
                QStringLiteral("protocol.class.custom")));
            continue;
        }

        const int required = it.value();
        if (given < required) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("Protocol %1 expects at least %2 argument(s) but got %3")
                    .arg(toks[2]).arg(required).arg(given),
                QStringLiteral("protocol.class.args"),
                QStringLiteral("Check the parameters required by %1.").arg(cls)));
        }
    }

    return protocolCount;
}

} // namespace OpenC3::ViewModels::Validation
