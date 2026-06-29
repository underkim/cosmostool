#include "TargetConfigParser.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace OpenC3::ViewModels::Validation
{
namespace
{

struct ParameterRule
{
    int minArgs;
    int maxArgs; // -1 means unbounded.
    QString hint;
};

const QHash<QString, ParameterRule> kTargetRules = {
    {"LANGUAGE", {1, 1, "LANGUAGE requires one language identifier such as ruby."}},
    {"REQUIRE", {1, 1, "REQUIRE requires exactly one Ruby file path."}},
    {"COMMANDS", {1, 1, "COMMANDS requires one command definition file path."}},
    {"TELEMETRY", {1, 1, "TELEMETRY requires one telemetry definition file path."}},
    {"IGNORE_PARAMETER", {2, 2, "IGNORE_PARAMETER requires packet and parameter names."}},
    {"IGNORE_ITEM", {2, 2, "IGNORE_ITEM requires packet and item names."}},
    {"LIMITS_GROUP",
     {1, -1, "LIMITS_GROUP requires a group name and optional targets/items."}},
    {"AUTO_SCREEN_SUBSTITUTE",
     {2, 2, "AUTO_SCREEN_SUBSTITUTE requires source and replacement text."}},
    {"STALENESS_SECONDS", {1, 1, "STALENESS_SECONDS requires one numeric value."}},
    {"META", {2, -1, "META requires a key and value."}}};

const QSet<QString> kPathKeywords = {"REQUIRE", "COMMANDS", "TELEMETRY"};
const QRegularExpression kNamePattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
const QRegularExpression kPathPattern(QStringLiteral(R"(^[A-Za-z0-9_./\\-]+$)"));

void addDiagnostic(ValidationReport& report, const QString& filePath, int line,
                   Severity severity, const QString& message, const QString& suggestion,
                   const QString& rule)
{
    report.diagnostics.append({filePath, line, severity, message, suggestion, rule});
}

bool isNumeric(const QString& value)
{
    bool ok = false;
    (void)value.toDouble(&ok);
    return ok;
}

} // namespace

QStringList TargetConfigParser::tokenize(const QString& line)
{
    QStringList tokens;
    bool inQuote = false;
    QChar quoteChar;
    QString current;

    for (int i = 0; i < line.length(); ++i)
    {
        const QChar c = line[i];
        if ((c == '\'' || c == '"') && (!inQuote || quoteChar == c))
        {
            if (inQuote)
            {
                tokens.append(current);
                current.clear();
                inQuote = false;
                quoteChar = QChar();
            }
            else
            {
                if (!current.isEmpty())
                {
                    tokens.append(current);
                    current.clear();
                }
                inQuote = true;
                quoteChar = c;
            }
        }
        else if ((c == ' ' || c == '\t') && !inQuote)
        {
            if (!current.isEmpty())
            {
                tokens.append(current);
                current.clear();
            }
        }
        else if (c == '#' && !inQuote)
        {
            break;
        }
        else
        {
            current += c;
        }
    }

    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

ValidationReport TargetConfigParser::validate(const QString& content,
                                              const QString& filePath)
{
    ValidationReport report;
    QSet<QString> commandFiles;
    QSet<QString> telemetryFiles;
    const QStringList lines = content.split('\n');

    for (int i = 0; i < lines.size(); ++i)
    {
        const int lineNo = i + 1;
        const QString trimmed = lines[i].trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList tokens = tokenize(trimmed);
        if (tokens.isEmpty())
            continue;

        const QString keyword = tokens[0].toUpper();
        const int argCount = tokens.size() - 1;
        const auto ruleIt = kTargetRules.constFind(keyword);
        if (ruleIt == kTargetRules.constEnd())
        {
            addDiagnostic(report, filePath, lineNo, Severity::Warning,
                          QString("Unknown target.txt keyword '%1'.").arg(tokens[0]),
                          "Check the target.txt keyword spelling or add a parser rule "
                          "for this COSMOS directive.",
                          "target.keyword.unknown");
            continue;
        }

        const ParameterRule rule = ruleIt.value();
        if (argCount < rule.minArgs || (rule.maxArgs >= 0 && argCount > rule.maxArgs))
        {
            const QString expected =
                rule.maxArgs < 0
                    ? QString("at least %1 argument(s)").arg(rule.minArgs)
                    : QString("%1-%2 argument(s)").arg(rule.minArgs).arg(rule.maxArgs);
            addDiagnostic(report, filePath, lineNo, Severity::Error,
                          QString("%1 has %2 argument(s), expected %3.")
                              .arg(keyword)
                              .arg(argCount)
                              .arg(expected),
                          rule.hint, "target.keyword.arity");
        }

        if (kPathKeywords.contains(keyword))
        {
            const QString path = tokens.value(1);
            if (!path.isEmpty() && !kPathPattern.match(path).hasMatch())
            {
                addDiagnostic(
                    report, filePath, lineNo, Severity::Error,
                    QString("%1 path '%2' contains unsupported characters.")
                        .arg(keyword, path),
                    "Use a relative COSMOS target path without shell metacharacters.",
                    "target.path.format");
            }
        }

        if (keyword == "COMMANDS")
        {
            const QString path = tokens.value(1);
            if (!commandFiles.isEmpty() && commandFiles.contains(path))
            {
                addDiagnostic(
                    report, filePath, lineNo, Severity::Warning,
                    QString("COMMANDS file '%1' is listed more than once.").arg(path),
                    "Remove duplicate COMMANDS entries to avoid loading the same "
                    "definitions repeatedly.",
                    "target.commands.duplicate");
            }
            commandFiles.insert(path);
        }
        else if (keyword == "TELEMETRY")
        {
            const QString path = tokens.value(1);
            if (!telemetryFiles.isEmpty() && telemetryFiles.contains(path))
            {
                addDiagnostic(
                    report, filePath, lineNo, Severity::Warning,
                    QString("TELEMETRY file '%1' is listed more than once.").arg(path),
                    "Remove duplicate TELEMETRY entries to avoid loading the same "
                    "definitions repeatedly.",
                    "target.telemetry.duplicate");
            }
            telemetryFiles.insert(path);
        }
        else if (keyword == "IGNORE_PARAMETER" || keyword == "IGNORE_ITEM")
        {
            for (int tokenIndex = 1; tokenIndex < tokens.size(); ++tokenIndex)
            {
                const QString name = tokens[tokenIndex];
                if (!kNamePattern.match(name).hasMatch())
                {
                    addDiagnostic(
                        report, filePath, lineNo, Severity::Error,
                        QString("%1 name '%2' is not a valid COSMOS identifier.")
                            .arg(keyword, name),
                        "Use packet and item names containing letters, numbers, and "
                        "underscores.",
                        "target.ignore.name");
                }
            }
        }
        else if (keyword == "STALENESS_SECONDS")
        {
            const QString value = tokens.value(1);
            if (!value.isEmpty() && !isNumeric(value))
            {
                addDiagnostic(
                    report, filePath, lineNo, Severity::Error,
                    QString("STALENESS_SECONDS value '%1' is not numeric.").arg(value),
                    "Use a number of seconds, for example STALENESS_SECONDS 30.",
                    "target.staleness.numeric");
            }
        }
    }

    if (commandFiles.isEmpty())
    {
        addDiagnostic(report, filePath, 0, Severity::Info,
                      "target.txt does not list any COMMANDS files.",
                      "Add COMMANDS entries if this target accepts commands.",
                      "target.commands.missing");
    }
    if (telemetryFiles.isEmpty())
    {
        addDiagnostic(report, filePath, 0, Severity::Info,
                      "target.txt does not list any TELEMETRY files.",
                      "Add TELEMETRY entries if this target publishes telemetry.",
                      "target.telemetry.missing");
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
