#pragma once

#include <QString>
#include <QVector>

#include <utility>

namespace OpenC3::ViewModels::Validation {

// ── Diagnostic ────────────────────────────────────────────────────────────────
// One finding produced by a validator. Always carries enough context for the UI
// to point the user at the exact place that is wrong and (when possible) suggest
// how to fix it.

struct Diagnostic {
    enum class Severity { Error, Warning, Info };

    Severity severity{Severity::Error};
    QString  filePath;     // empty when validating in-memory content
    int      line{0};      // 1-based; 0 means file-level (no specific line)
    int      column{0};    // 1-based; 0 means unknown
    QString  message;      // what is wrong
    QString  suggestion;   // optional: how to fix it
    QString  rule;         // short machine code, e.g. "screen.end.unmatched"

    [[nodiscard]] static Diagnostic error(int line, QString message,
                                          QString rule = {}, QString suggestion = {});
    [[nodiscard]] static Diagnostic warning(int line, QString message,
                                            QString rule = {}, QString suggestion = {});
    [[nodiscard]] static Diagnostic info(int line, QString message,
                                         QString rule = {}, QString suggestion = {});

    [[nodiscard]] QString severityLabel() const;
};

// ── ValidationReport ──────────────────────────────────────────────────────────
// Aggregates diagnostics across one or many files.

class ValidationReport {
public:
    QVector<Diagnostic> diagnostics;

    void add(Diagnostic diagnostic) { diagnostics.append(std::move(diagnostic)); }
    void merge(const ValidationReport& other);

    // Stamp filePath onto every diagnostic that does not yet have one.
    void setFilePath(const QString& path);

    [[nodiscard]] int  errorCount()   const;
    [[nodiscard]] int  warningCount() const;
    [[nodiscard]] int  infoCount()    const;
    [[nodiscard]] bool ok()           const { return errorCount() == 0; }

    // Human-readable one-line summary, e.g. "2 errors, 1 warning".
    [[nodiscard]] QString summary() const;
};

} // namespace OpenC3::ViewModels::Validation
