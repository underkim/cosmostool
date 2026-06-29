#pragma once

#include "Diagnostic.h"

#include <QString>

namespace OpenC3::ViewModels::Validation {

// Shared PROTOCOL line checker — the single source of truth for protocol rules.
// Scans `content` for PROTOCOL lines (which live inside an INTERFACE/ROUTER),
// appending findings for bad direction, wrong context, and built-in protocol
// class argument counts. Returns the number of PROTOCOL lines seen so callers
// can decide whether to add a "no protocols" note.
//
// Used by ProtocolValidator (standalone) and PluginValidator (whole plugin.txt)
// so there is exactly one implementation of protocol validation.
int checkProtocolLines(const QString& content, ValidationReport& report);

} // namespace OpenC3::ViewModels::Validation
