#include "DoctorViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QColor>

namespace OpenC3::ViewModels {

// ── HealthCheckTableModel ─────────────────────────────────────────────────────

HealthCheckTableModel::HealthCheckTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void HealthCheckTableModel::setResults(
    std::vector<Models::HealthCheckResult> results)
{
    beginResetModel();
    results_ = std::move(results);
    endResetModel();
}

int HealthCheckTableModel::rowCount(const QModelIndex& p) const
{
    return p.isValid() ? 0 : static_cast<int>(results_.size());
}

int HealthCheckTableModel::columnCount(const QModelIndex& p) const
{
    return p.isValid() ? 0 : Column::ColCount;
}

QVariant HealthCheckTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& r = results_[static_cast<std::size_t>(index.row())];

    auto statusText = [](Models::HealthStatus s) -> QString {
        switch (s) {
            case Models::HealthStatus::Pass:    return "PASS";
            case Models::HealthStatus::Warning: return "WARN";
            case Models::HealthStatus::Fail:    return "FAIL";
            case Models::HealthStatus::Skipped: return "SKIP";
        }
        return {};
    };

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Column::Category: return QString::fromStdString(r.category);
            case Column::Name:     return QString::fromStdString(r.name);
            case Column::Status:   return statusText(r.status);
            case Column::Detail:   return QString::fromStdString(r.detail);
            default: return {};
        }
    }

    if (role == Qt::ForegroundRole && index.column() == Column::Status) {
        switch (r.status) {
            case Models::HealthStatus::Pass:    return QColor(0x4e, 0xc9, 0x4e);
            case Models::HealthStatus::Warning: return QColor(0xcd, 0x91, 0x00);
            case Models::HealthStatus::Fail:    return QColor(0xf1, 0x44, 0x4c);
            default:                            return QColor(0x9d, 0x9d, 0x9d);
        }
    }

    if (role == Qt::ToolTipRole && index.column() == Column::Detail)
        return QString::fromStdString(r.suggestion);

    return {};
}

QVariant HealthCheckTableModel::headerData(
    int section, Qt::Orientation orient, int role) const
{
    if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case Column::Category: return "Category";
        case Column::Name:     return "Check";
        case Column::Status:   return "Result";
        case Column::Detail:   return "Detail";
        default:               return {};
    }
}

int HealthCheckTableModel::passCount() const noexcept
{
    int n = 0;
    for (const auto& r : results_) if (r.passed()) ++n;
    return n;
}

int HealthCheckTableModel::warningCount() const noexcept
{
    int n = 0;
    for (const auto& r : results_) if (r.warned()) ++n;
    return n;
}

int HealthCheckTableModel::failCount() const noexcept
{
    int n = 0;
    for (const auto& r : results_) if (r.failed()) ++n;
    return n;
}

// ── DoctorViewModel ───────────────────────────────────────────────────────────

DoctorViewModel::DoctorViewModel(
    Services::IDoctorService& doctor, QObject* parent)
    : ViewModelBase(parent)
    , doctor_(doctor)
    , resultsModel_(new HealthCheckTableModel(this))
{}

HealthCheckTableModel* DoctorViewModel::resultsModel() const noexcept
{
    return resultsModel_;
}

int DoctorViewModel::passCount()    const noexcept { return resultsModel_->passCount();    }
int DoctorViewModel::warningCount() const noexcept { return resultsModel_->warningCount(); }
int DoctorViewModel::failCount()    const noexcept { return resultsModel_->failCount();    }

void DoctorViewModel::runAllChecks()
{
    setLoading(true);
    clearError();

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished,
            watcher, &QObject::deleteLater);

    auto future = QtConcurrent::run([this] {
        std::vector<Models::HealthCheckResult> results;

        [[maybe_unused]] auto report = doctor_.runAll([this, &results](const Models::HealthCheckResult& r) {
            results.push_back(r);
            QString statusStr;
            switch (r.status) {
                case Models::HealthStatus::Pass:    statusStr = "PASS"; break;
                case Models::HealthStatus::Warning: statusStr = "WARN"; break;
                case Models::HealthStatus::Fail:    statusStr = "FAIL"; break;
                default:                            statusStr = "SKIP"; break;
            }
            QMetaObject::invokeMethod(this, [this,
                name = QString::fromStdString(r.name),
                st   = statusStr] {
                    emit checkProgressUpdated(name, st);
                }, Qt::QueuedConnection);
        });

        QMetaObject::invokeMethod(this, [this, r = std::move(results)] () mutable {
            resultsModel_->setResults(std::move(r));
            emit summaryChanged();
            emit refreshed();
            setLoading(false);
        }, Qt::QueuedConnection);
    });
    watcher->setFuture(future);
}

} // namespace OpenC3::ViewModels
