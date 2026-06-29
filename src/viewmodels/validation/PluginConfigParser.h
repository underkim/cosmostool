#pragma once

#include "validation/Diagnostic.h"

#include <QString>
#include <QStringList>

namespace OpenC3::ViewModels::Validation {

class PluginConfigParser {
public:
    [[nodiscard]] static ValidationReport validate(const QString& content,
                                                   const QString& filePath = QStringLiteral("plugin.txt"));

private:
    static QStringList tokenize(const QString& line);
};

} // namespace OpenC3::ViewModels::Validation
