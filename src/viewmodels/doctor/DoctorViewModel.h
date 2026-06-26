#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/doctor/IDoctorService.h"
#include "models/HealthCheckResult.h"

#include <QAbstractTableModel>
#include <vector>

namespace OpenC3::ViewModels {

class HealthCheckTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Category = 0, Name, Status, Detail, ColCount };

    explicit HealthCheckTableModel(QObject* parent = nullptr);
    void setResults(std::vector<Models::HealthCheckResult> results);

    [[nodiscard]] int      rowCount(const QModelIndex& = {})    const override;
    [[nodiscard]] int      columnCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role)   const override;
    [[nodiscard]] QVariant headerData(int, Qt::Orientation, int) const override;

    [[nodiscard]] int passCount()    const noexcept;
    [[nodiscard]] int warningCount() const noexcept;
    [[nodiscard]] int failCount()    const noexcept;

private:
    std::vector<Models::HealthCheckResult> results_;
};

// ─────────────────────────────────────────────────────────────────────────────

class DoctorViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(int passCount    READ passCount    NOTIFY summaryChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY summaryChanged)
    Q_PROPERTY(int failCount    READ failCount    NOTIFY summaryChanged)

public:
    explicit DoctorViewModel(
        Services::IDoctorService& doctor,
        QObject*                  parent = nullptr);

    [[nodiscard]] HealthCheckTableModel* resultsModel() const noexcept;
    [[nodiscard]] int passCount()    const noexcept;
    [[nodiscard]] int warningCount() const noexcept;
    [[nodiscard]] int failCount()    const noexcept;

public slots:
    void runAllChecks();

signals:
    void summaryChanged();
    void checkProgressUpdated(const QString& checkName,
                              const QString& status);

private:
    Services::IDoctorService& doctor_;
    HealthCheckTableModel*    resultsModel_{nullptr};
};

} // namespace OpenC3::ViewModels
