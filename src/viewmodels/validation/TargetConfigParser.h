#pragma once

#include <QString>
#include <QStringList>

#include "validation/Diagnostic.h"

namespace OpenC3::ViewModels::Validation
{

class TargetConfigParser
{
  public:
    [[nodiscard]] static ValidationReport
    validate(const QString& content,
             const QString& filePath = QStringLiteral("target.txt"));

  private:
    static QStringList tokenize(const QString& line);
};

} // namespace OpenC3::ViewModels::Validation
