#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── TargetConfigValidator ─────────────────────────────────────────────────────
// Offline validator for a COSMOS target.txt file — the per-target settings that
// live next to a target's cmd/tlm definitions.
//
// Checks the recognised target.txt keywords (LANGUAGE, REQUIRE, COMMANDS,
// TELEMETRY, IGNORE_PARAMETER, IGNORE_ITEM, the *_UNIQUE_ID_MODE flags and the
// command/telemetry log settings), validates the argument shape of the common
// ones, and flags unknown keywords as warnings so typos surface.

class TargetConfigValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("target"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Target (target.txt)"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
