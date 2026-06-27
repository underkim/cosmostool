#pragma once

#include "viewmodels/dashboard/DashboardViewModel.h"
#include "viewmodels/docker/DockerViewModel.h"
#include "viewmodels/doctor/DoctorViewModel.h"
#include "viewmodels/settings/SettingsViewModel.h"
#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmViewModel.h"
#include "viewmodels/packettools/PacketToolsViewModel.h"
#include "viewmodels/logviewer/LogViewerViewModel.h"
#include "viewmodels/infra/InfraViewModel.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QStatusBar>

namespace OpenC3::UI {

/// Application main window.
///
/// Implements a navigation rail (sidebar) + content area layout.
/// Each module is a full-page view inside a QStackedWidget.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(
        ViewModels::DashboardViewModel&    dashboard,
        ViewModels::DockerViewModel&       docker,
        ViewModels::InfraViewModel&        infra,
        ViewModels::DoctorViewModel&       doctor,
        ViewModels::SettingsViewModel&     settings,
        ViewModels::PluginViewModel&       plugin,
        ViewModels::CmdTlmViewModel&       cmdTlm,
        ViewModels::PacketToolsViewModel&  packetTools,
        ViewModels::LogViewerViewModel&    logViewer,
        QWidget*                           parent = nullptr);

    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNavItemSelected(int index);
    void onConnectionStatusChanged();

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupNavigation();
    void setupViews();
    void connectSignals();

    ViewModels::DashboardViewModel&    dashboardVm_;
    ViewModels::DockerViewModel&       dockerVm_;
    ViewModels::InfraViewModel&        infraVm_;
    ViewModels::DoctorViewModel&       doctorVm_;
    ViewModels::SettingsViewModel&     settingsVm_;
    ViewModels::PluginViewModel&       pluginVm_;
    ViewModels::CmdTlmViewModel&       cmdTlmVm_;
    ViewModels::PacketToolsViewModel&  packetToolsVm_;
    ViewModels::LogViewerViewModel&    logViewerVm_;

    QWidget*        centralWidget_{nullptr};
    QListWidget*    navRail_{nullptr};
    QStackedWidget* contentStack_{nullptr};

    QLabel* connectionLabel_{nullptr};
    QLabel* dockerLabel_{nullptr};
};

} // namespace OpenC3::UI
