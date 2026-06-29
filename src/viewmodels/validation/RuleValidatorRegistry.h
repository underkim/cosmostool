#pragma once

#include "IRuleValidator.h"

#include <QString>
#include <QVector>

namespace OpenC3::ViewModels::Validation {

// ── RuleValidatorRegistry ─────────────────────────────────────────────────────
// Central catalogue of every standalone rule validator. The UI enumerates these
// to let the user pick a single rule to run; ConfigValidator uses appliesTo()
// for auto-detection. Pointers are owned by the registry (function-local
// statics) and live for the program's lifetime.

namespace RuleValidatorRegistry {

// All registered validators, in display order.
[[nodiscard]] const QVector<const IRuleValidator*>& all();

// Look up a validator by its stable id (e.g. "protocol"); nullptr if unknown.
[[nodiscard]] const IRuleValidator* byId(const QString& id);

} // namespace RuleValidatorRegistry

} // namespace OpenC3::ViewModels::Validation
