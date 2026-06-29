#pragma once

#include "IRuleValidator.h"

namespace OpenC3::ViewModels::Validation {

// ── ProtocolValidator ─────────────────────────────────────────────────────────
// Deep, standalone validator for PROTOCOL lines (which live inside an INTERFACE
// or ROUTER). Beyond the direction/context checks PluginConfigParser performs,
// this knows the built-in COSMOS protocol classes and their required parameters:
//
//   PROTOCOL <READ|WRITE|READ_WRITE> <protocol_class[.rb]> <args...>
//
// Checks:
//   - the direction token is READ / WRITE / READ_WRITE,
//   - a protocol class is given,
//   - PROTOCOL appears inside an INTERFACE / ROUTER block,
//   - for recognised built-in classes, the minimum number of required arguments
//     is present (e.g. TerminatedProtocol needs write+read termination chars),
//   - unrecognised classes are reported as info (likely a custom protocol).

class ProtocolValidator final : public IRuleValidator {
public:
    [[nodiscard]] QString id() const override    { return QStringLiteral("protocol"); }
    [[nodiscard]] QString label() const override { return QStringLiteral("Protocol (PROTOCOL)"); }

    [[nodiscard]] bool appliesTo(const QString& path,
                                 const QString& content) const override;
    [[nodiscard]] ValidationReport validate(const QString& content) const override;
};

} // namespace OpenC3::ViewModels::Validation
