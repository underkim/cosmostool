#include "MainWindow.h"
#include "views/DashboardView.h"
#include "views/DockerView.h"
#include "views/DoctorView.h"
#include "views/SettingsView.h"
#include "views/PluginView.h"
#include "views/InfraView.h"
#include "views/CmdTlmView.h"
#include "views/PacketToolsView.h"
#include "views/LogViewerView.h"
#include "views/ValidatorView.h"
#include "dialogs/AboutDialog.h"
#include "dialogs/UserGuideDialog.h"
#include "dialogs/ConnectionDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QCloseEvent>
#include <QApplication>
#include <QFont>
#include <QTabWidget>

namespace OpenC3::UI {

namespace {
// Row/stack indices for the navigation rail. Kept in one place so the rail,
// the content stack, and the quick-action handlers cannot drift apart.
enum NavIndex {
    NavHome = 0,
    NavWorkspace,
    NavCmdTlm,
    NavPacketTools,
    NavLogs,
    NavSettings,
    NavAdvanced,
};
} // namespace

MainWindow::MainWindow(
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
    QWidget*                           parent)
    : QMainWindow(parent)
    , dashboardVm_(dashboard)
    , dockerVm_(docker)
    , infraVm_(infra)
    , doctorVm_(doctor)
    , settingsVm_(settings)
    , pluginVm_(plugin)
    , cmdTlmVm_(cmdTlm)
    , packetToolsVm_(packetTools)
    , logViewerVm_(logViewer)
    , validatorVm_(validator)
{
    setWindowTitle("OpenC3 Developer Toolkit");
    setMinimumSize(1280, 800);
    resize(1440, 900);

    setupUi();
    setupMenuBar();
    setupStatusBar();
    connectSignals();
}

void MainWindow::setupUi()
{
    centralWidget_ = new QWidget(this);
    setCentralWidget(centralWidget_);

    auto* mainLayout = new QHBoxLayout(centralWidget_);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupNavigation();

    contentStack_ = new QStackedWidget(this);
    setupViews();

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(navRail_);
    splitter->addWidget(contentStack_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({200, 1240});
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);

    mainLayout->addWidget(splitter);
}

void MainWindow::setupNavigation()
{
    navRail_ = new QListWidget(this);
    navRail_->setFixedWidth(200);
    navRail_->setFrameShape(QFrame::NoFrame);
    navRail_->setSpacing(2);

    auto addItem = [&](const QString& text) {
        auto* item = new QListWidgetItem(text);
        item->setSizeHint(QSize(200, 40));
        QFont f = item->font();
        f.setPointSize(11);
        item->setFont(f);
        navRail_->addItem(item);
    };

    addItem("Home");         // 0
    addItem("Workspace");    // 1
    addItem("CMD / TLM");    // 2
    addItem("Packet Tools"); // 3
    addItem("Logs");         // 4
    addItem("Settings");     // 5
    addItem("Advanced");     // 6

    navRail_->setCurrentRow(0);
    navRail_->setObjectName("navRail");

    connect(navRail_, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavItemSelected);
}

void MainWindow::setupViews()
{
    auto* dashboardView = new Views::DashboardView(dashboardVm_, this);
    auto* pluginView    = new Views::PluginView(pluginVm_, infraVm_, cmdTlmVm_, validatorVm_, this);
    auto* cmdTlmView    = new Views::CmdTlmView(cmdTlmVm_, this);
    auto* validatorView = new Views::ValidatorView(validatorVm_, this);

    // Advanced groups the less-common tools so the main rail stays short. The
    // features are not removed — just tucked behind one entry.
    auto* advancedTabs = new QTabWidget(this);
    advancedTabs->setObjectName("PluginDetailTabs");
    advancedTabs->addTab(new Views::DockerView(dockerVm_, this), "Docker"); // 0
    advancedTabs->addTab(new Views::InfraView(infraVm_, this),   "Infra");  // 1
    advancedTabs->addTab(new Views::DoctorView(doctorVm_, this), "Doctor"); // 2
    advancedTabs->addTab(validatorView,                          "Validator"); // 3
    constexpr int kAdvancedDoctorTab    = 2;
    constexpr int kAdvancedValidatorTab = 3;

    // ── Navigation helpers ──────────────────────────────────────────────────
    auto goTo = [this](int row) {
        navRail_->setCurrentRow(row);   // also switches the stack via the signal
        contentStack_->setCurrentIndex(row);
    };

    // CMD / TLM is now a top-level page (index 2).
    connect(pluginView, &Views::PluginView::openCmdTlmRequested,
            this, [goTo, cmdTlmView](const QString& remoteFilePath) {
                goTo(NavCmdTlm);
                cmdTlmView->openFile(remoteFilePath);
            });

    auto toValidator = [this, goTo, validatorView, advancedTabs,
                        kAdvancedValidatorTab](const QString& content) {
        goTo(NavAdvanced);
        advancedTabs->setCurrentIndex(kAdvancedValidatorTab);
        validatorView->checkContent(content);
    };
    connect(cmdTlmView, &Views::CmdTlmView::openInValidatorRequested, this, toValidator);
    connect(pluginView, &Views::PluginView::openInValidatorRequested, this, toValidator);

    // ── Home (Dashboard) quick actions ──────────────────────────────────────
    connect(dashboardView, &Views::DashboardView::connectRequested,
            this, &MainWindow::showConnectionDialog);
    connect(dashboardView, &Views::DashboardView::runDoctorRequested,
            this, [this, goTo, advancedTabs, kAdvancedDoctorTab] {
                if (!settingsVm_.isConnected()) {
                    showConnectionDialog();
                    if (!settingsVm_.isConnected())
                        return;
                }
                goTo(NavAdvanced);
                advancedTabs->setCurrentIndex(kAdvancedDoctorTab);
                doctorVm_.runAllChecks();
            });
    connect(dashboardView, &Views::DashboardView::openWorkspaceRequested,
            this, [goTo] { goTo(NavWorkspace); });
    connect(dashboardView, &Views::DashboardView::openCmdTlmRequested,
            this, [goTo] { goTo(NavCmdTlm); });
    connect(dashboardView, &Views::DashboardView::openPacketToolsRequested,
            this, [goTo] { goTo(NavPacketTools); });
    connect(dashboardView, &Views::DashboardView::openLogsRequested,
            this, [goTo] { goTo(NavLogs); });

    contentStack_->addWidget(dashboardView);                                    // 0 Home
    contentStack_->addWidget(pluginView);                                       // 1 Workspace
    contentStack_->addWidget(cmdTlmView);                                       // 2 CMD / TLM
    contentStack_->addWidget(new Views::PacketToolsView(packetToolsVm_, this)); // 3 Packet Tools
    contentStack_->addWidget(new Views::LogViewerView(logViewerVm_, this));     // 4 Logs
    contentStack_->addWidget(new Views::SettingsView(settingsVm_, this));       // 5 Settings
    contentStack_->addWidget(advancedTabs);                                     // 6 Advanced
}

void MainWindow::showConnectionDialog()
{
    Dialogs::ConnectionDialog dlg(settingsVm_, this);
    dlg.exec();
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu   = menuBar()->addMenu("&File");
    auto* exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);

    auto* viewMenu      = menuBar()->addMenu("&View");
    auto* refreshAction = new QAction("&Refresh", this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, [this] {
        const int idx = contentStack_->currentIndex();
        if (idx == 0) {
            dashboardVm_.refresh();
        } else if (idx == 1) {
            pluginVm_.refresh();
        }
    });
    viewMenu->addAction(refreshAction);

    auto* helpMenu = menuBar()->addMenu("&Help");

    auto* guideAction = new QAction("&User Guide", this);
    guideAction->setShortcut(QKeySequence::HelpContents);
    connect(guideAction, &QAction::triggered, this, [this] {
        auto* dlg = new Dialogs::UserGuideDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    helpMenu->addAction(guideAction);

    helpMenu->addSeparator();

    auto* aboutAction = new QAction("&About", this);
    connect(aboutAction, &QAction::triggered, this, [this] {
        Dialogs::AboutDialog dlg(this);
        dlg.exec();
    });
    helpMenu->addAction(aboutAction);
}

void MainWindow::setupStatusBar()
{
    connectionLabel_ = new QLabel("  Disconnected  ", this);
    dockerLabel_     = new QLabel("  Docker: --  ", this);

    statusBar()->addWidget(connectionLabel_);
    statusBar()->addWidget(dockerLabel_);
    statusBar()->setObjectName("statusBar");
}

void MainWindow::connectSignals()
{
    connect(&dashboardVm_, &ViewModels::DashboardViewModel::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(&dashboardVm_, &ViewModels::DashboardViewModel::dockerStatusChanged,
            this, [this] {
                dockerLabel_->setText("  Docker: " + dashboardVm_.dockerStatus() + "  ");
            });

    onConnectionStatusChanged();
    dockerLabel_->setText("  Docker: " + dashboardVm_.dockerStatus() + "  ");
}

void MainWindow::onNavItemSelected(int index)
{
    contentStack_->setCurrentIndex(index);
}

void MainWindow::onConnectionStatusChanged()
{
    const QString status = dashboardVm_.connectionStatus();
    connectionLabel_->setText("  " + status + "  ");
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    logViewerVm_.stopStream();
    settingsVm_.disconnect();
    event->accept();
}

} // namespace OpenC3::UI
