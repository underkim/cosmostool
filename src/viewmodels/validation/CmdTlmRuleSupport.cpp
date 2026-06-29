#include "CmdTlmRuleSupport.h"

namespace OpenC3::ViewModels::Validation {

namespace {

using CmdTlmDiagnostic = OpenC3::ViewModels::CmdTlmDiagnostic;

// Convert a CMD/TLM diagnostic into the common diagnostic type.
Diagnostic fromCmdTlm(const CmdTlmDiagnostic& src)
{
    const bool isError = src.severity == CmdTlmDiagnostic::Severity::Error;
    return isError ? Diagnostic::error(src.line, src.message, QStringLiteral("cmdtlm"))
                   : Diagnostic::warning(src.line, src.message, QStringLiteral("cmdtlm"));
}

} // namespace

ValidationReport validateCmdTlmScoped(
    const QString& content,
    std::optional<CmdTlmDiagnostic::Scope> only)
{
    ValidationReport report;
    const auto parsed = OpenC3::ViewModels::CmdTlmParser::parse(content);

    for (const auto& d : parsed.diagnostics) {
        // Keep block-less structural issues (Any) regardless, plus findings that
        // match the requested scope.
        if (only && d.scope != CmdTlmDiagnostic::Scope::Any && d.scope != *only)
            continue;
        report.add(fromCmdTlm(d));
    }
    return report;
}

} // namespace OpenC3::ViewModels::Validation
