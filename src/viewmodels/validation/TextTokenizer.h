#pragma once

#include <QString>
#include <QStringList>

namespace OpenC3::ViewModels::Validation {

// Split a single configuration line into whitespace-separated tokens, keeping
// double-quoted strings (which may contain spaces) as a single token. Matches
// the tokenization used by the COSMOS configuration parser.
[[nodiscard]] QStringList tokenizeConfigLine(const QString& line);

} // namespace OpenC3::ViewModels::Validation
