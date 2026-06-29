#pragma once

#include "Diagnostic.h"

#include <QString>

namespace OpenC3::ViewModels::Validation {

// ── IRuleValidator ────────────────────────────────────────────────────────────
// One standalone, offline validator for a single COSMOS rule family (CMD, TLM,
// SCREEN, PROTOCOL, plugin.txt, …). Each validator can be run on its own so the
// UI can offer "validate just this rule" alongside auto-detection.
//
// Validators are stateless and cheap; the registry hands out shared singletons.

class IRuleValidator {
public:
    virtual ~IRuleValidator() = default;

    // Stable machine id, e.g. "cmd", "tlm", "screen", "protocol", "plugin".
    [[nodiscard]] virtual QString id() const = 0;

    // Human-readable label for the UI, e.g. "Command (CMD)".
    [[nodiscard]] virtual QString label() const = 0;

    // Best-effort guess of whether this validator is the natural fit for the
    // given file/content (used by auto-detect). path may be empty.
    [[nodiscard]] virtual bool appliesTo(const QString& path,
                                         const QString& content) const = 0;

    // Validate in-memory content and return all findings.
    [[nodiscard]] virtual ValidationReport validate(const QString& content) const = 0;
};

} // namespace OpenC3::ViewModels::Validation
