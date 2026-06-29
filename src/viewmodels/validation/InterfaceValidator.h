#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── InterfaceValidator ────────────────────────────────────────────────────────
// Standalone validator focused on INTERFACE / ROUTER declarations and the
// modifiers that attach to them (including PROTOCOL, via the shared protocol
// rule). Lets the user check just the connectivity portion of a plugin.txt
// without the full plugin sweep. Shares its keyword lists with
// PluginConfigParser through PluginKeywords.

class InterfaceValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("interface"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Interface / Router"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
