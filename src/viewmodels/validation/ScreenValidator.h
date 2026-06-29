#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── ScreenValidator ───────────────────────────────────────────────────────────
// Standalone wrapper around ScreenParser so screen rules can be selected and run
// on their own through the registry/UI.

class ScreenValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("screen"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Screen"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
