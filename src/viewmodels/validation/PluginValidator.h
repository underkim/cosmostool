#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── PluginValidator ───────────────────────────────────────────────────────────
// Standalone wrapper around PluginConfigParser for whole-plugin.txt validation
// (TARGET / INTERFACE / ROUTER / MICROSERVICE / TOOL and their modifiers).
// Protocol-specific depth lives in ProtocolValidator; this covers the rest.

class PluginValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("plugin"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Plugin (plugin.txt)"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
