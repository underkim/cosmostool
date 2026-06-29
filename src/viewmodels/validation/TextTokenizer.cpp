#include "TextTokenizer.h"

namespace OpenC3::ViewModels::Validation {

QStringList tokenizeConfigLine(const QString& line)
{
    QStringList tokens;
    bool        inQuote = false;
    QString     current;

    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (inQuote) {
                tokens.append(current);
                current.clear();
                inQuote = false;
            } else {
                if (!current.isEmpty()) {
                    tokens.append(current);
                    current.clear();
                }
                inQuote = true;
            }
        } else if ((c == ' ' || c == '\t') && !inQuote) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.isEmpty())
        tokens.append(current);

    return tokens;
}

} // namespace OpenC3::ViewModels::Validation
