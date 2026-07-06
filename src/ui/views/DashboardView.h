#pragma once

#include "ui/AppMode.h"
#include "viewmodels/dashboard/DashboardViewModel.h"
#include "ui/widgets/MetricCard.h"
#include "ui/widgets/StatusBadge.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

#include <array>

namespace OpenC3::UI::Views {

class DashboardView final : public QWidget {
    Q_OBJECT
public:
    explicit DashboardView(
        ViewModels::DashboardViewModel& vm,
        QWidget* parent = nullptr);

    // Plugin Creation: no numbered "Connect" step is shown at all - connecting
    // happens transparently via MainWindow::autoConnectIfNeeded(), so telling
    // the user to "connect first" would contradict that. Connect & Operate:
    // keeps the existing 3-step guidance, with step 3 pointing at Check &
    // Build instead of Workspace (not reachable from this mode).
    void setAppMode(AppMode mode);

signals:
    // Quick-action requests. The MainWindow owns navigation and the connection
    // dialog, so the Home page only asks for these actions; it does not perform
    // navigation itself.
    void connectRequested();
    void runDoctorRequested();
    void openWorkspaceRequested();
    void openCheckBuildRequested();
    void openValidatorRequested();
    void openPacketToolsRequested();
    void openLogsRequested();

private:
    void setupUi();
    void bindViewModel();
    void updateHomeGuidance();
    void onRecommendedActionClicked();

    ViewModels::DashboardViewModel& vm_;
    AppMode appMode_{AppMode::PluginCreation};

    QLabel*               guidanceLabel_{nullptr};
    QPushButton*          recommendedActionBtn_{nullptr};
    QFrame*               stepCardsRow_{nullptr};
    std::array<QPushButton*, 3> stepButtons_{};
    std::array<QLabel*, 3> stepTitleLabels_{};
    std::array<QLabel*, 3> stepDescLabels_{};
    std::array<Widgets::StatusBadge*, 3> stepBadges_{};
    Widgets::StatusBadge* connectionBadge_{nullptr};
    Widgets::StatusBadge* dockerBadge_{nullptr};
    QLabel*               versionLabel_{nullptr};
    QLabel*               containerLabel_{nullptr};
    Widgets::MetricCard*  cpuCard_{nullptr};
    Widgets::MetricCard*  memCard_{nullptr};
    Widgets::MetricCard*  diskCard_{nullptr};
};

} // namespace OpenC3::UI::Views
