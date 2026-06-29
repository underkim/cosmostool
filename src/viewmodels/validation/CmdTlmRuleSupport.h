#pragma once

#include "Diagnostic.h"

#include "viewmodels/cmdtlm/CmdTlmParser.h"

#include <QString>

#include <optional>

namespace OpenC3::ViewModels::Validation {

// Shared helper for the CMD/TLM rule validators. Runs the single CmdTlmParser
// engine over `content`, then converts its findings into the common Diagnostic
// type. When `only` is set, findings are filtered to that block scope (block-less
// structural issues, tagged Scope::Any, always pass through); when unset every
// finding is kept (used by the auto-detect / folder path).
[[nodiscard]] ValidationReport validateCmdTlmScoped(
    const QString& content,
    std::optional<OpenC3::ViewModels::CmdTlmDiagnostic::Scope> only);

} // namespace OpenC3::ViewModels::Validation
