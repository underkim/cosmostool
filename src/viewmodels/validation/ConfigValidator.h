#pragma once

#include "validation/Diagnostic.h"

#include <QString>

namespace OpenC3::ViewModels::Validation {

enum class FileKind {
    Unknown,
    PluginConfig,
    TargetConfig
};

class ConfigValidator {
public:
    [[nodiscard]] static FileKind classify(const QString& filePath);
    [[nodiscard]] static ValidationReport validateContent(const QString& content,
                                                          const QString& filePath);
    [[nodiscard]] static ValidationReport validateFolder(const QString& folderPath);
};

} // namespace OpenC3::ViewModels::Validation
