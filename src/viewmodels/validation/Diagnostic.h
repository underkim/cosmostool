#pragma once

#include <QString>
#include <QVector>

namespace OpenC3::ViewModels::Validation {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
    QString filePath;
    int     line{0};
    Severity severity{Severity::Info};
    QString message;
    QString suggestion;
    QString rule;
};

struct ValidationReport {
    QVector<Diagnostic> diagnostics;

    [[nodiscard]] int errorCount() const
    {
        int count = 0;
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == Severity::Error) ++count;
        }
        return count;
    }

    [[nodiscard]] int warningCount() const
    {
        int count = 0;
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == Severity::Warning) ++count;
        }
        return count;
    }

    [[nodiscard]] bool hasErrors() const { return errorCount() > 0; }
};

} // namespace OpenC3::ViewModels::Validation
