#pragma once

#include "Diagnostic.h"

#include <QString>

namespace OpenC3::ViewModels::Validation {

// ── ScreenParser ──────────────────────────────────────────────────────────────
// Static, offline validator for COSMOS telemetry screen definition files.
//
// Checks:
//   - the file begins with a SCREEN <width> <height> <polling> header,
//   - layout widgets (VERTICAL, HORIZONTAL, ...) are balanced with END,
//   - SETTING/SUBSETTING modifiers follow a widget,
//   - telemetry-bound widgets carry the minimum target/packet/item arguments,
//   - unknown widgets are flagged (as warnings) so typos surface.
//
// It does not execute the screen; it only checks structure and syntax.

class ScreenParser {
public:
    [[nodiscard]] static ValidationReport parse(const QString& content);
};

} // namespace OpenC3::ViewModels::Validation
