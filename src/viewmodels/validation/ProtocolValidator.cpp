#include "ProtocolValidator.h"
#include "ProtocolRuleSupport.h"
#include "TextTokenizer.h"

#include <QStringList>

namespace OpenC3::ViewModels::Validation {

bool ProtocolValidator::appliesTo(const QString& /*path*/, const QString& content) const
{
    const QStringList lines = content.split('\n');
    for (const QString& raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.startsWith('#'))
            continue;
        const QStringList toks = tokenizeConfigLine(trimmed);
        if (!toks.isEmpty() && toks[0].toUpper() == QStringLiteral("PROTOCOL"))
            return true;
    }
    return false;
}

ValidationReport ProtocolValidator::validate(const QString& content) const
{
    ValidationReport report;
    const int protocolCount = checkProtocolLines(content, report);

    if (protocolCount == 0) {
        report.add(Diagnostic::info(
            0, QStringLiteral("No PROTOCOL lines found"),
            QStringLiteral("protocol.none")));
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
