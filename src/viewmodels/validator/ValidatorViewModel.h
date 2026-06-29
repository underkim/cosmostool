#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "viewmodels/validation/ConfigValidator.h"

#include <QPair>
#include <QString>
#include <QVector>

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

    // (id, label) of every standalone rule validator, for the UI to enumerate.
    [[nodiscard]] QVector<QPair<QString, QString>> availableValidators() const;

public slots:
    void validateFile(const QString& path);
    void validateFolder(const QString& dir);
    // validatorId selects a single rule validator by id (e.g. "cmd", "protocol").
    // An empty id means auto-detect the file kind from the content.
    void validateTextWith(const QString& validatorId, const QString& content);
    void clear();

signals:
    void reportReady();

private:
    Validation::ValidationReport report_;
    QString                      lastSource_;
};

} // namespace OpenC3::ViewModels
