#include "Diagnostic.h"

#include <QStringList>

namespace OpenC3::ViewModels::Validation {

Diagnostic Diagnostic::error(int line, QString message, QString rule, QString suggestion)
{
    Diagnostic d;
    d.severity   = Severity::Error;
    d.line       = line;
    d.message    = std::move(message);
    d.rule       = std::move(rule);
    d.suggestion = std::move(suggestion);
    return d;
}

Diagnostic Diagnostic::warning(int line, QString message, QString rule, QString suggestion)
{
    Diagnostic d;
    d.severity   = Severity::Warning;
    d.line       = line;
    d.message    = std::move(message);
    d.rule       = std::move(rule);
    d.suggestion = std::move(suggestion);
    return d;
}

Diagnostic Diagnostic::info(int line, QString message, QString rule, QString suggestion)
{
    Diagnostic d;
    d.severity   = Severity::Info;
    d.line       = line;
    d.message    = std::move(message);
    d.rule       = std::move(rule);
    d.suggestion = std::move(suggestion);
    return d;
}

QString Diagnostic::severityLabel() const
{
    switch (severity) {
    case Severity::Error:   return QStringLiteral("Error");
    case Severity::Warning: return QStringLiteral("Warning");
    case Severity::Info:    return QStringLiteral("Info");
    }
    return QStringLiteral("Info");
}

void ValidationReport::merge(const ValidationReport& other)
{
    diagnostics.append(other.diagnostics);
}

void ValidationReport::setFilePath(const QString& path)
{
    for (auto& d : diagnostics) {
        if (d.filePath.isEmpty())
            d.filePath = path;
    }
}

int ValidationReport::errorCount() const
{
    int n = 0;
    for (const auto& d : diagnostics)
        if (d.severity == Diagnostic::Severity::Error) ++n;
    return n;
}

int ValidationReport::warningCount() const
{
    int n = 0;
    for (const auto& d : diagnostics)
        if (d.severity == Diagnostic::Severity::Warning) ++n;
    return n;
}

int ValidationReport::infoCount() const
{
    int n = 0;
    for (const auto& d : diagnostics)
        if (d.severity == Diagnostic::Severity::Info) ++n;
    return n;
}

QString ValidationReport::summary() const
{
    const int errors   = errorCount();
    const int warnings = warningCount();
    const int infos    = infoCount();

    QStringList parts;
    parts << QStringLiteral("%1 error%2").arg(errors).arg(errors == 1 ? "" : "s");
    parts << QStringLiteral("%1 warning%2").arg(warnings).arg(warnings == 1 ? "" : "s");
    if (infos > 0)
        parts << QStringLiteral("%1 info").arg(infos);
    return parts.join(QStringLiteral(", "));
}

} // namespace OpenC3::ViewModels::Validation
