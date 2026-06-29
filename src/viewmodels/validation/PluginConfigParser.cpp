#include "PluginConfigParser.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace OpenC3::ViewModels::Validation {
namespace {

struct ParameterRule {
    int minArgs;
    int maxArgs; // -1 means unbounded.
    QString hint;
};

const QHash<QString, ParameterRule> kPluginRules = {
    {"VARIABLE", {2, 2, "VARIABLE requires a name and a default value."}},
    {"TARGET", {1, 2, "TARGET requires a target folder and optionally a substituted display name."}},
    {"INTERFACE", {2, -1, "INTERFACE requires a name and interface class plus constructor arguments."}},
    {"ROUTER", {2, -1, "ROUTER requires a name and router class plus constructor arguments."}},
    {"PROTOCOL", {1, -1, "PROTOCOL requires a protocol class plus constructor arguments."}},
    {"MAP_TARGET", {1, 1, "MAP_TARGET requires exactly one target name."}},
    {"CMD_TLM_SERVER", {1, 1, "CMD_TLM_SERVER requires exactly one target name."}},
    {"MICROSERVICE", {2, -1, "MICROSERVICE requires a name and class or command."}},
    {"WIDGET", {2, -1, "WIDGET requires a widget name and implementation."}},
    {"TAB", {2, -1, "TAB requires a tab name and implementation."}},
    {"TOOL", {2, -1, "TOOL requires a tool name and implementation."}},
    {"CONVERSION", {2, -1, "CONVERSION requires a conversion name and implementation."}},
    {"LIMITS_RESPONSE", {2, -1, "LIMITS_RESPONSE requires a response name and implementation."}},
    {"ACCESSOR", {2, -1, "ACCESSOR requires an accessor name and implementation."}},
    {"SECRET", {1, 2, "SECRET requires a key and optional default or prompt."}},
    {"ENV", {2, 2, "ENV requires a key and value."}},
    {"OPTION", {2, -1, "OPTION requires a name and default value; optional metadata may follow."}},
    {"REQUIRE", {1, 1, "REQUIRE requires exactly one Ruby file path."}},
    {"GEM", {1, 2, "GEM requires a gem name and optional version constraint."}}
};

const QSet<QString> kVariableAwareKeywords = {
    "TARGET", "INTERFACE", "ROUTER", "MAP_TARGET", "CMD_TLM_SERVER", "MICROSERVICE",
    "WIDGET", "TAB", "TOOL", "CONVERSION", "LIMITS_RESPONSE", "ACCESSOR", "ENV", "OPTION",
    "PROTOCOL"
};

const QRegularExpression kIdentifierPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
const QRegularExpression kVariableReferencePattern(QStringLiteral(R"(<%=\s*([A-Za-z_][A-Za-z0-9_]*)\s*%>)"));
const QRegularExpression kEnvNamePattern(QStringLiteral("^[A-Z_][A-Z0-9_]*$"));
const QRegularExpression kClassOrFilePattern(QStringLiteral(R"((\.rb$)|(^[A-Za-z_][A-Za-z0-9_:]*$))"));

void addDiagnostic(ValidationReport& report,
                   const QString& filePath,
                   int line,
                   Severity severity,
                   const QString& message,
                   const QString& suggestion,
                   const QString& rule)
{
    report.diagnostics.append({filePath, line, severity, message, suggestion, rule});
}

bool isVariableReference(const QString& value)
{
    return kVariableReferencePattern.match(value).hasMatch();
}

QString stripVariableSuffix(const QString& value)
{
    QString result = value;
    const QRegularExpressionMatch match = kVariableReferencePattern.match(result);
    if (match.hasMatch()) {
        result.replace(match.captured(0), match.captured(1));
    }
    return result;
}

} // namespace

QStringList PluginConfigParser::tokenize(const QString& line)
{
    QStringList tokens;
    bool inQuote = false;
    bool inErb = false;
    QChar quoteChar;
    QString current;

    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line[i];
        if (!inQuote && !inErb && c == '<' && i + 2 < line.length() && line[i + 1] == '%' && line[i + 2] == '=') {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            inErb = true;
            current += c;
        } else if (inErb) {
            current += c;
            if (c == '>' && i > 0 && line[i - 1] == '%') {
                tokens.append(current.trimmed());
                current.clear();
                inErb = false;
            }
        } else if ((c == '\'' || c == '"') && (!inQuote || quoteChar == c)) {
            if (inQuote) {
                tokens.append(current);
                current.clear();
                inQuote = false;
                quoteChar = QChar();
            } else {
                if (!current.isEmpty()) {
                    tokens.append(current);
                    current.clear();
                }
                inQuote = true;
                quoteChar = c;
            }
        } else if ((c == ' ' || c == '\t') && !inQuote) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else if (c == '#' && !inQuote) {
            break;
        } else {
            current += c;
        }
    }

    if (!current.isEmpty()) tokens.append(current);
    return tokens;
}

ValidationReport PluginConfigParser::validate(const QString& content, const QString& filePath)
{
    ValidationReport report;
    QSet<QString> variables;
    QString currentAttachable;
    const QStringList lines = content.split('\n');

    for (int i = 0; i < lines.size(); ++i) {
        const int lineNo = i + 1;
        const QString trimmed = lines[i].trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;

        const QStringList tokens = tokenize(trimmed);
        if (tokens.isEmpty()) continue;

        const QString keyword = tokens[0].toUpper();
        const int argCount = tokens.size() - 1;

        if (keyword == "VARIABLE") {
            if (tokens.size() > 1) {
                const QString variableName = tokens[1];
                if (!kIdentifierPattern.match(variableName).hasMatch()) {
                    addDiagnostic(report, filePath, lineNo, Severity::Error,
                                  QString("Invalid VARIABLE name '%1'.").arg(variableName),
                                  "Use a Ruby/ERB-friendly identifier such as target_name.",
                                  "plugin.variable.name");
                } else {
                    variables.insert(variableName);
                }
            }
        }

        const auto ruleIt = kPluginRules.constFind(keyword);
        if (ruleIt == kPluginRules.constEnd()) {
            addDiagnostic(report, filePath, lineNo, Severity::Warning,
                          QString("Unknown plugin.txt keyword '%1'.").arg(tokens[0]),
                          "Check the COSMOS plugin.txt keyword spelling or add a parser rule for this keyword.",
                          "plugin.keyword.unknown");
            continue;
        }

        const ParameterRule rule = ruleIt.value();
        if (argCount < rule.minArgs || (rule.maxArgs >= 0 && argCount > rule.maxArgs)) {
            const QString expected = rule.maxArgs < 0
                ? QString("at least %1 argument(s)").arg(rule.minArgs)
                : QString("%1-%2 argument(s)").arg(rule.minArgs).arg(rule.maxArgs);
            addDiagnostic(report, filePath, lineNo, Severity::Error,
                          QString("%1 has %2 argument(s), expected %3.").arg(keyword).arg(argCount).arg(expected),
                          rule.hint,
                          "plugin.keyword.arity");
        }

        if (keyword == "PROTOCOL" && currentAttachable.isEmpty()) {
            addDiagnostic(report, filePath, lineNo, Severity::Error,
                          "PROTOCOL appears before any INTERFACE or ROUTER.",
                          "Place PROTOCOL on an indented line after the INTERFACE or ROUTER it configures.",
                          "plugin.protocol.parent");
        }

        if (keyword == "INTERFACE" || keyword == "ROUTER") {
            currentAttachable = tokens.value(1);
        } else if (keyword != "PROTOCOL") {
            currentAttachable.clear();
        }

        if (keyword == "OPTION" || keyword == "SECRET") {
            const QString name = tokens.value(1);
            if (!name.isEmpty() && !kIdentifierPattern.match(name).hasMatch()) {
                addDiagnostic(report, filePath, lineNo, Severity::Error,
                              QString("%1 name '%2' is not a valid identifier.").arg(keyword, name),
                              "Use letters, numbers, and underscores; start with a letter or underscore.",
                              "plugin.option_secret.name");
            }
        }

        if (keyword == "ENV") {
            const QString envName = tokens.value(1);
            if (!envName.isEmpty() && !kEnvNamePattern.match(envName).hasMatch()) {
                addDiagnostic(report, filePath, lineNo, Severity::Warning,
                              QString("ENV name '%1' is not conventional uppercase snake case.").arg(envName),
                              "Use names such as COSMOS_MY_SETTING for portable environment variables.",
                              "plugin.env.name");
            }
        }

        if ((keyword == "INTERFACE" || keyword == "ROUTER") && tokens.size() > 2) {
            const QString implementation = tokens[2];
            if (!kClassOrFilePattern.match(implementation).hasMatch()) {
                addDiagnostic(report, filePath, lineNo, Severity::Warning,
                              QString("%1 implementation '%2' does not look like a Ruby class or .rb file.").arg(keyword, implementation),
                              "Use a Ruby class name, namespaced class, or a Ruby file ending in .rb.",
                              "plugin.implementation.format");
            }
        }

        if (keyword == "PROTOCOL" && tokens.size() > 1) {
            const QString implementation = tokens[1];
            if (!kClassOrFilePattern.match(implementation).hasMatch()) {
                addDiagnostic(report, filePath, lineNo, Severity::Warning,
                              QString("PROTOCOL implementation '%1' does not look like a Ruby class or .rb file.").arg(implementation),
                              "Use a Ruby class name, namespaced class, or a Ruby file ending in .rb.",
                              "plugin.implementation.format");
            }
        }

        if (kVariableAwareKeywords.contains(keyword)) {
            for (int tokenIndex = 1; tokenIndex < tokens.size(); ++tokenIndex) {
                const QString token = tokens[tokenIndex];
                if (!isVariableReference(token)) continue;

                const QString variableName = stripVariableSuffix(token);
                if (!variables.contains(variableName)) {
                    addDiagnostic(report, filePath, lineNo, Severity::Error,
                                  QString("Undefined VARIABLE '%1' used in '%2'.").arg(variableName, token),
                                  "Declare the variable earlier with VARIABLE before referencing it with ERB substitution.",
                                  "plugin.variable.undefined");
                }
            }
        }
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
