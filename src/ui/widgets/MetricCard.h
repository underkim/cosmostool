#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>

namespace OpenC3::UI::Widgets {

/// A reusable card widget that displays a metric name, value, and progress bar.
/// Used on the Dashboard for CPU / Memory / Disk.
class MetricCard final : public QWidget {
    Q_OBJECT

public:
    explicit MetricCard(const QString& title, const QString& unit,
                        QWidget* parent = nullptr);

    void setValue(double percent, const QString& label = {});

private:
    QLabel*       titleLabel_{nullptr};
    QLabel*       valueLabel_{nullptr};
    QProgressBar* bar_{nullptr};
    QString       unit_;
};

} // namespace OpenC3::UI::Widgets
