#include "ScreenValidator.h"
#include "ScreenParser.h"
#include "TextTokenizer.h"

#include <QFileInfo>
#include <QStringList>

namespace OpenC3::ViewModels::Validation {

bool ScreenValidator::appliesTo(const QString& path, const QString& content) const
{
    const QStringList lines = content.split('\n');
    for (const QString& raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        const QStringList toks = tokenizeConfigLine(trimmed);
        if (!toks.isEmpty())
            return toks[0].toUpper() == QStringLiteral("SCREEN");
    }
    // Fall back to the standard COSMOS layout hint.
    const QString lowered = path.toLower();
    return lowered.contains(QStringLiteral("/screens/"))
        || lowered.contains(QStringLiteral("\\screens\\"));
}

ValidationReport ScreenValidator::validate(const QString& content) const
{
    return ScreenParser::parse(content);
}

} // namespace OpenC3::ViewModels::Validation
