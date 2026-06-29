#include "TargetConfigValidator.h"
#include "TextTokenizer.h"

#include <QFileInfo>
#include <QHash>
#include <QStringList>

namespace OpenC3::ViewModels::Validation {

namespace {

// Recognised target.txt keywords mapped to the minimum number of *arguments*
// (tokens after the keyword) they require. Keywords not listed are reported as
// unknown so typos surface without failing valid files.
const QHash<QString, int>& targetKeywords()
{
    static const QHash<QString, int> kKeywords = {
        { "LANGUAGE",            1 }, // ruby | python
        { "REQUIRE",             1 }, // file to require
        { "COMMANDS",            1 }, // command definition file
        { "TELEMETRY",           1 }, // telemetry definition file
        { "IGNORE_PARAMETER",    1 },
        { "IGNORE_ITEM",         1 },
        { "CMD_UNIQUE_ID_MODE",  0 },
        { "TLM_UNIQUE_ID_MODE",  0 },
        { "ERB_SUFFIX",          1 },
        { "DISABLE_ERB",         0 },
        // Command/telemetry log + buffer settings (shared with plugin TARGET).
        { "CMD_BUFFER_DEPTH",            1 },
        { "CMD_LOG_CYCLE_TIME",          1 },
        { "CMD_LOG_CYCLE_SIZE",          1 },
        { "CMD_LOG_RETAIN_TIME",         1 },
        { "CMD_DECOM_LOG_CYCLE_TIME",    1 },
        { "CMD_DECOM_LOG_CYCLE_SIZE",    1 },
        { "CMD_DECOM_LOG_RETAIN_TIME",   1 },
        { "TLM_BUFFER_DEPTH",            1 },
        { "TLM_LOG_CYCLE_TIME",          1 },
        { "TLM_LOG_CYCLE_SIZE",          1 },
        { "TLM_LOG_RETAIN_TIME",         1 },
        { "TLM_DECOM_LOG_CYCLE_TIME",    1 },
        { "TLM_DECOM_LOG_CYCLE_SIZE",    1 },
        { "TLM_DECOM_LOG_RETAIN_TIME",   1 },
        { "REDUCED_MINUTE_LOG_RETAIN_TIME", 1 },
        { "REDUCED_HOUR_LOG_RETAIN_TIME",   1 },
        { "REDUCED_DAY_LOG_RETAIN_TIME",    1 },
        { "LOG_RETAIN_TIME",             1 },
        { "REDUCER_DISABLE",             0 },
        { "REDUCER_MAX_CPU_UTILIZATION", 1 },
        { "SHARD",                       1 },
    };
    return kKeywords;
}

} // namespace

bool TargetConfigValidator::appliesTo(const QString& path, const QString& /*content*/) const
{
    return QFileInfo(path).fileName().compare(QStringLiteral("target.txt"),
                                              Qt::CaseInsensitive) == 0;
}

ValidationReport TargetConfigValidator::validate(const QString& content) const
{
    ValidationReport report;
    const QStringList lines = content.split('\n');

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = tokenizeConfigLine(trimmed);
        if (toks.isEmpty())
            continue;

        const QString kw = toks[0].toUpper();

        const auto it = targetKeywords().constFind(kw);
        if (it == targetKeywords().constEnd()) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("Unknown target.txt keyword '%1'").arg(toks[0]),
                QStringLiteral("target.keyword.unknown")));
            continue;
        }

        const int provided = static_cast<int>(toks.size()) - 1;
        if (provided < it.value()) {
            report.add(Diagnostic::warning(
                lineNo,
                QStringLiteral("%1 expects at least %2 argument(s) but got %3")
                    .arg(kw).arg(it.value()).arg(provided),
                QStringLiteral("target.keyword.args")));
        }

        // LANGUAGE must be ruby or python.
        if (kw == QStringLiteral("LANGUAGE") && provided >= 1) {
            const QString lang = toks[1].toLower();
            if (lang != QStringLiteral("ruby") && lang != QStringLiteral("python")) {
                report.add(Diagnostic::warning(
                    lineNo,
                    QStringLiteral("LANGUAGE '%1' should be ruby or python").arg(toks[1]),
                    QStringLiteral("target.language.value")));
            }
        }
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
