#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── CommandValidator ──────────────────────────────────────────────────────────
// Validates only the COMMAND side of a CMD/TLM definition. Runs the shared
// CmdTlmParser and keeps the findings scoped to COMMAND blocks (plus block-less
// structural issues). TELEMETRY-only findings are filtered out so the user can
// focus on command definitions in isolation.

class CommandValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("cmd"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Command (CMD)"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
