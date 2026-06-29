#include "CommandValidator.h"
#include "CmdTlmRuleSupport.h"
#include "TextTokenizer.h"

#include <QStringList>

namespace OpenC3::ViewModels::Validation {

bool CommandValidator::appliesTo(const QString& /*path*/, const QString& content) const
{
    const QStringList lines = content.split('\n');
    for (const QString& raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        const QStringList toks = tokenizeConfigLine(trimmed);
        if (!toks.isEmpty() && toks[0].toUpper() == QStringLiteral("COMMAND"))
            return true;
    }
    return false;
}

ValidationReport CommandValidator::validate(const QString& content) const
{
    return validateCmdTlmScoped(content,
                                OpenC3::ViewModels::CmdTlmDiagnostic::Scope::Command);
}

} // namespace OpenC3::ViewModels::Validation
