#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "viewmodels/validation/ConfigValidator.h"

#include <QString>

namespace OpenC3::ViewModels {

// ── ValidatorViewModel ────────────────────────────────────────────────────────
// Thin MVVM wrapper around the offline ConfigValidator. Validation is pure,
// fast, in-process static analysis, so it runs synchronously on the GUI thread
// and exposes the resulting report to the view.
class ValidatorViewModel final : public ViewModelBase {
    Q_OBJECT

public:
    explicit ValidatorViewModel(QObject* parent = nullptr);

    [[nodiscard]] const Validation::ValidationReport& report() const noexcept;
    [[nodiscard]] QString lastSource() const noexcept;

public slots:
    void validateFile(const QString& path);
    void validateFolder(const QString& dir);
    // kindOrAuto: -1 auto-detect, otherwise a ConfigValidator::FileKind value.
    void validateText(const QString& content, int kindOrAuto);
    void clear();

signals:
    void reportReady();

private:
    Validation::ValidationReport report_;
    QString                      lastSource_;
};

} // namespace OpenC3::ViewModels
