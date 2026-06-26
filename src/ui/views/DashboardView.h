#pragma once

#include "viewmodels/dashboard/DashboardViewModel.h"
#include "ui/widgets/MetricCard.h"
#include "ui/widgets/StatusBadge.h"

#include <QWidget>
#include <QLabel>

namespace OpenC3::UI::Views {

class DashboardView final : public QWidget {
    Q_OBJECT
public:
    explicit DashboardView(
        ViewModels::DashboardViewModel& vm,
        QWidget* parent = nullptr);

private:
    void setupUi();
    void bindViewModel();

    ViewModels::DashboardViewModel& vm_;

    Widgets::StatusBadge* connectionBadge_{nullptr};
    Widgets::StatusBadge* dockerBadge_{nullptr};
    QLabel*               versionLabel_{nullptr};
    QLabel*               containerLabel_{nullptr};
    Widgets::MetricCard*  cpuCard_{nullptr};
    Widgets::MetricCard*  memCard_{nullptr};
    Widgets::MetricCard*  diskCard_{nullptr};
};

} // namespace OpenC3::UI::Views
