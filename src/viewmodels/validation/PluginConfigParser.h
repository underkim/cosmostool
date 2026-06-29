#pragma once

#include "Diagnostic.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace OpenC3::ViewModels::Validation {

// ── PluginConfigParser ────────────────────────────────────────────────────────
// Static, offline validator for a COSMOS plugin.txt definition.
//
// Checks structure and syntax of TARGET / INTERFACE / ROUTER / MICROSERVICE
// declarations and their modifiers, including:
//   - PROTOCOL lines have a READ/WRITE/READ_WRITE direction and live inside an
//     INTERFACE or ROUTER,
//   - INTERFACE/ROUTER/MICROSERVICE/TARGET carry their required arguments,
//   - MAP_TARGET references a declared TARGET,
//   - VARIABLE declarations are well-formed.
//
// File existence of referenced targets is checked separately by ConfigValidator
// when a plugin directory is available (built-in COSMOS classes are not local).

class PluginConfigParser {
public:
    struct Result {
        ValidationReport report;
        QStringList      declaredTargets;     // TARGET names
        QStringList      declaredInterfaces;  // INTERFACE / ROUTER names
        QStringList      targetFolders;       // TARGET <folder> values
        QVector<int>     targetFolderLines;   // line of each TARGET declaration
    };

    [[nodiscard]] static Result parse(const QString& content);
};

} // namespace OpenC3::ViewModels::Validation
