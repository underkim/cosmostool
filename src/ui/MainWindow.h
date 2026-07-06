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
#include "viewmodels/validator/ValidatorViewModel.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QVector>

class QAction;

namespace OpenC3::UI::Views { class PluginView; }

namespace OpenC3::UI {

/// Which top-level workflow the app is currently focused on.
/// PluginCreation: Home, Workspace (plugin/file/edit only) - no connection
/// required up front, connects transparently in the background.
/// ConnectOperate: Environment, Validator, Packet Tools, Logs, plus
/// Workspace's Check & Build & Install step - requires a real connection.
enum class AppMode { PluginCreation, ConnectOperate };

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
        ViewModels::ValidatorViewModel&    validator,
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
    void showConnectionDialog();

    // App mode: Plugin Creation (Home, Workspace, Settings - no connection required
    // up front) vs. Connect & Operate (Environment/Validator/Packet Tools/Logs, plus
    // Workspace's Check & Build & Install step). Nothing is removed - just hidden -
    // and any programmatic navigation to a row hidden by the current mode
    // transparently switches mode first.
    void setAppMode(AppMode mode);
    void applyNavVisibility();
    [[nodiscard]] bool rowVisibleInMode(int row, AppMode mode) const noexcept;

    ViewModels::DashboardViewModel&    dashboardVm_;
    ViewModels::DockerViewModel&       dockerVm_;
    ViewModels::InfraViewModel&        infraVm_;
    ViewModels::DoctorViewModel&       doctorVm_;
    ViewModels::SettingsViewModel&     settingsVm_;
    ViewModels::PluginViewModel&       pluginVm_;
    ViewModels::CmdTlmViewModel&       cmdTlmVm_;
    ViewModels::PacketToolsViewModel&  packetToolsVm_;
    ViewModels::LogViewerViewModel&    logViewerVm_;
    ViewModels::ValidatorViewModel&    validatorVm_;

    QWidget*        centralWidget_{nullptr};
    QListWidget*    navRail_{nullptr};
    QStackedWidget* contentStack_{nullptr};
    Views::PluginView* pluginView_{nullptr};

    // Maps nav-rail row index -> contentStack_ page index. Most rows map to
    // their own dedicated page, but the "Check & Build" row shares the same
    // PluginView page as "Workspace" (steps 4-5 vs. 1-3 of the same wizard),
    // so row index and page index are no longer interchangeable. Built once
    // in setupViews().
    QVector<int> rowToStackPage_;

    // Clickable: opens the connection dialog from anywhere in the app.
    QPushButton* connectionButton_{nullptr};
    QLabel*      dockerLabel_{nullptr};

    AppMode      appMode_{AppMode::PluginCreation};
    QAction*     pluginCreationAction_{nullptr};
    QAction*     connectOperateAction_{nullptr};
    QPushButton* modeToggleBtn_{nullptr};
};

} // namespace OpenC3::UI
