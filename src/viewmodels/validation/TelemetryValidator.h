#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── TelemetryValidator ────────────────────────────────────────────────────────
// Mirror of CommandValidator for the TELEMETRY side. Runs the shared
// CmdTlmParser and keeps only TELEMETRY-scoped findings (plus block-less
// structural issues), hiding COMMAND-only diagnostics.

class TelemetryValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("tlm"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Telemetry (TLM)"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
