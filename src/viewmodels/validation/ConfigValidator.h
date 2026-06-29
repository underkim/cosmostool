#pragma once

#include "Diagnostic.h"

#include <QString>

namespace OpenC3::ViewModels::Validation {

// ── ConfigValidator ───────────────────────────────────────────────────────────
// Entry point that ties the individual validators together. It classifies a
// COSMOS configuration file by its name/content, dispatches to the right
// validator (CMD/TLM, screen, or plugin.txt), and can sweep an entire plugin or
// target folder in one pass — all offline, without a COSMOS runtime.

class ConfigValidator {
public:
    enum class FileKind { CmdTlm, Screen, PluginConfig, Unknown };

    // Best-effort classification from the path and a peek at the content.
    [[nodiscard]] static FileKind classify(const QString& path, const QString& content);

    // Validate in-memory content of a known kind. filePath is left empty.
    [[nodiscard]] static ValidationReport validateContent(FileKind kind,
                                                          const QString& content);

    // Read and validate a single file. Diagnostics are stamped with the path.
    [[nodiscard]] static ValidationReport validateFile(const QString& path);

    // Recursively validate every recognised config file under a directory and
    // perform cross-file checks (e.g. TARGET folders referenced by plugin.txt).
    [[nodiscard]] static ValidationReport validateFolder(const QString& dir);

    [[nodiscard]] static QString fileKindLabel(FileKind kind);
};

} // namespace OpenC3::ViewModels::Validation
