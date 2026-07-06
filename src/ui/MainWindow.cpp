#include "MainWindow.h"
#include "views/DashboardView.h"
#include "views/DockerView.h"
#include "views/DoctorView.h"
#include "views/SettingsView.h"
#include "views/PluginView.h"
#include "views/InfraView.h"
#include "views/PacketToolsView.h"
#include "views/LogViewerView.h"
#include "views/ValidatorView.h"
#include "dialogs/AboutDialog.h"
#include "dialogs/UserGuideDialog.h"
#include "dialogs/ConnectionDialog.h"
#include "models/ConnectionProfile.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QApplication>
#include <QFont>
#include <QTabWidget>
#include <QShortcut>
#include <QKeySequence>
#include <QSettings>
#include <QMessageBox>
#include <QUuid>

namespace OpenC3::UI {

namespace {
// Nav rail row indices. NavCheckBuild is a second nav entry onto the *same*
// PluginView page as NavWorkspace (steps 4-5 vs. 1-3) - row index and content
// stack page index are therefore no longer 1:1; see rowToStackPage_.
enum NavIndex {
    NavHome = 0,
    NavWorkspace,
    NavSettings,
    NavEnvironment,
    NavValidator,
    NavPacketTools,
    NavLogs,
    NavCheckBuild,
};
} // namespace

AppMode MainWindow::loadPersistedAppMode()
{
    QSettings s;
    return s.value("MainWindow/appMode", "pluginCreation").toString() == "connectOperate"
        ? AppMode::ConnectOperate : AppMode::PluginCreation;
}

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
    setWindowTitle(tr("OpenC3 Developer Toolkit"));
    setMinimumSize(1280, 800);
    resize(1440, 900);

    QSettings windowSettings;
    appMode_ = loadPersistedAppMode();

    setupUi();
    setupMenuBar();
    setupStatusBar();
    connectSignals();

    // Applied last, once navRail_/pluginCreationAction_/connectOperateAction_/
    // modeToggleBtn_ all exist, so their state can be synced against appMode_
    // at the same time.
    applyNavVisibility();
    autoConnectIfNeeded();

    const QVariant geometry = windowSettings.value("MainWindow/geometry");
    if (geometry.isValid()) restoreGeometry(geometry.toByteArray());
    const QVariant state = windowSettings.value("MainWindow/state");
    if (state.isValid()) restoreState(state.toByteArray());
}

void MainWindow::setupUi()
{
    centralWidget_ = new QWidget(this);
    setCentralWidget(centralWidget_);

    auto* mainLayout = new QHBoxLayout(centralWidget_);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupNavigation();

    // Wrap navRail_ with the mode toggle button so it travels with the rail
    // rather than floating separately in the layout.
    auto* navPanel       = new QWidget(this);
    auto* navPanelLayout = new QVBoxLayout(navPanel);
    navPanelLayout->setContentsMargins(0, 0, 0, 0);
    navPanelLayout->setSpacing(0);
    navPanelLayout->addWidget(navRail_, 1);
    navPanelLayout->addWidget(modeToggleBtn_);

    contentStack_ = new QStackedWidget(this);
    contentStack_->setObjectName("contentStack");
    setupViews();

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(navPanel);
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
    // A fixed-width vertical list of short single-line labels never needs to
    // scroll horizontally - without this, Qt was reserving a useless
    // horizontal scrollbar strip at the bottom of the rail.
    navRail_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto addItem = [&](const QString& text, const QString& description) {
        const int row = navRail_->count();
        auto* item = new QListWidgetItem(text);
        item->setSizeHint(QSize(200, 40));
        QFont f = item->font();
        f.setPointSize(11);
        item->setFont(f);
        // Surface the page purpose and keyboard shortcut so each item is discoverable on hover.
        item->setToolTip(tr("%1 — %2 (Ctrl+%3)")
                             .arg(text)
                             .arg(description)
                             .arg(row + 1));
        navRail_->addItem(item);

        // Ctrl+<n> jumps straight to this section from anywhere in the app - switch
        // app mode first if the target row is currently hidden in the active mode,
        // same as goTo() does for programmatic navigation, so a shortcut never lands
        // on a page with no visible nav highlight.
        auto* shortcut = new QShortcut(
            QKeySequence(QStringLiteral("Ctrl+%1").arg(row + 1)), this);
        connect(shortcut, &QShortcut::activated,
                this, [this, row] {
                    if (!rowVisibleInMode(row, appMode_) && navRail_->item(row))
                        setAppMode(appMode_ == AppMode::PluginCreation
                            ? AppMode::ConnectOperate : AppMode::PluginCreation);
                    navRail_->setCurrentRow(row);
                });
    };

    addItem(tr("Home"), tr("Review status and jump to common workflows"));                 // 0  (Ctrl+1)
    addItem(tr("Workspace"), tr("Manage plugins, and browse, edit, and validate CMD/TLM files")); // 1  (Ctrl+2)
    addItem(tr("Settings"), tr("Manage connection profiles and preferences"));             // 2  (Ctrl+3)
    addItem(tr("Environment"), tr("Check Docker, containers, and environment configuration")); // 3  (Ctrl+4)
    addItem(tr("Validator"), tr("Run offline checks on CMD/TLM, screens, and plugin.txt")); // 4  (Ctrl+5)
    addItem(tr("Packet Tools"), tr("Simulate and inspect packet payloads"));              // 5  (Ctrl+6)
    addItem(tr("Logs"), tr("Inspect toolkit and OpenC3 logs"));                            // 6  (Ctrl+7)
    addItem(tr("Check & Build"), tr("Check, build, and install the plugin open in Workspace")); // 7  (Ctrl+8)

    navRail_->setCurrentRow(0);
    navRail_->setObjectName("navRail");

    connect(navRail_, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavItemSelected);

    // In-context toggle for the app mode - a menu item alone (setupMenuBar())
    // is too easy to miss for something this central to hiding/revealing nav rows.
    // appMode_ is already loaded from QSettings by the time this runs (set in
    // the constructor before setupUi()), so this reflects the persisted mode;
    // setAppMode() only re-syncs this text on later *changes* (it no-ops if
    // the mode passed in already matches appMode_), so the initial text has
    // to be set here explicitly.
    modeToggleBtn_ = new QPushButton(
        appMode_ == AppMode::PluginCreation
            ? tr("Switch to Connect && Operate") + " " + QString::fromUtf8("▸")
            : tr("Switch to Plugin Creation") + " " + QString::fromUtf8("◂"),
        this);
    modeToggleBtn_->setFlat(true);
    modeToggleBtn_->setObjectName("ModeToggleButton");
    modeToggleBtn_->setToolTip(appMode_ == AppMode::PluginCreation
        ? tr("Currently in Plugin Creation mode. Switch to see Environment, Validator, Packet Tools, and Logs.")
        : tr("Currently in Connect & Operate mode. Switch back to focus on plugin creation."));
    connect(modeToggleBtn_, &QPushButton::clicked, this, [this] {
        setAppMode(appMode_ == AppMode::PluginCreation
            ? AppMode::ConnectOperate : AppMode::PluginCreation);
    });
}

bool MainWindow::rowVisibleInMode(int row, AppMode mode) const noexcept
{
    // Home and Settings are common to both modes. Workspace (plugin creation,
    // no connection required up front) is Plugin Creation-only; Environment/
    // Validator/Packet Tools/Logs (all inherently connection-dependent) are
    // Connect & Operate-only.
    if (row == NavHome || row == NavSettings)
        return true;
    if (row == NavWorkspace)
        return mode == AppMode::PluginCreation;
    return mode == AppMode::ConnectOperate;
}

void MainWindow::applyNavVisibility()
{
    for (int row = 0; row < navRail_->count(); ++row) {
        if (navRail_->item(row))
            navRail_->item(row)->setHidden(!rowVisibleInMode(row, appMode_));
    }
}

void MainWindow::setAppMode(AppMode mode)
{
    if (mode == appMode_)
        return;
    appMode_ = mode;

    // Switching modes while sitting on a row the new mode hides would strand
    // the user on a page with no visible nav highlight and no way back - Qt's
    // setRowHidden doesn't move the current selection on its own.
    if (!rowVisibleInMode(navRail_->currentRow(), appMode_)) {
        navRail_->setCurrentRow(NavHome);
        contentStack_->setCurrentIndex(rowToStackPage_[NavHome]);
    }

    QSettings windowSettings;
    windowSettings.setValue("MainWindow/appMode",
        appMode_ == AppMode::ConnectOperate ? "connectOperate" : "pluginCreation");

    if (pluginCreationAction_) pluginCreationAction_->setChecked(appMode_ == AppMode::PluginCreation);
    if (connectOperateAction_) connectOperateAction_->setChecked(appMode_ == AppMode::ConnectOperate);
    if (modeToggleBtn_) {
        modeToggleBtn_->setText(appMode_ == AppMode::PluginCreation
            ? tr("Switch to Connect && Operate") + " " + QString::fromUtf8("▸")
            : tr("Switch to Plugin Creation") + " " + QString::fromUtf8("◂"));
        modeToggleBtn_->setToolTip(appMode_ == AppMode::PluginCreation
            ? tr("Currently in Plugin Creation mode. Switch to see Environment, Validator, Packet Tools, and Logs.")
            : tr("Currently in Connect & Operate mode. Switch back to focus on plugin creation."));
    }

    applyNavVisibility();
    if (dashboardView_) dashboardView_->setAppMode(appMode_);

    if (appMode_ == AppMode::PluginCreation) {
        autoConnectAttempted_ = false;
        autoConnectIfNeeded();
    }
}

void MainWindow::autoConnectIfNeeded()
{
    if (appMode_ != AppMode::PluginCreation) return;
    if (autoConnectAttempted_) return;
    if (settingsVm_.isConnected()) return;
    autoConnectAttempted_ = true;

    settingsVm_.loadProfiles();

    QString targetId = settingsVm_.defaultProfileId();
    if (targetId.isEmpty() && settingsVm_.profileModel()->rowCount() > 0) {
        if (const auto* first = settingsVm_.profileModel()->profileAt(0))
            targetId = QString::fromStdString(first->id);
    }

    if (!targetId.isEmpty()) {
        settingsVm_.connectToProfile(targetId);
        return;
    }

    // No saved profile at all - silently create+connect a Quick-WSL profile,
    // the same sequence as ConnectionDialog::onCreateWslProfile(), just
    // headless. If no WSL distro is detected either, leave the app
    // disconnected; the status-bar connectionButton_ already reads
    // "Disconnected" and doubles as the click-to-configure recovery path.
    const QStringList distros = Dialogs::detectWslDistros();
    if (distros.isEmpty())
        return;

    Models::ConnectionProfile p;
    p.id              = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    p.name            = QStringLiteral("WSL (%1)").arg(distros.first()).toStdString();
    p.mode            = Models::ConnectionMode::WSL;
    p.isDefault       = false;
    p.wslDistribution = distros.first().toStdString();
    p.cosmosRootPath  = "/cosmos";

    const QString profileId = QString::fromStdString(p.id);
    settingsVm_.saveProfile(p);
    settingsVm_.setDefaultProfile(profileId);
    settingsVm_.connectToProfile(profileId);
}

void MainWindow::setupViews()
{
    auto* dashboardView = new Views::DashboardView(dashboardVm_, this);
    auto* pluginView    = new Views::PluginView(pluginVm_, infraVm_, cmdTlmVm_, validatorVm_, logViewerVm_, this);
    auto* validatorView = new Views::ValidatorView(validatorVm_, this);
    pluginView_ = pluginView;
    dashboardView_ = dashboardView;
    dashboardView_->setAppMode(appMode_);
    // Matches the default AppMode::PluginCreation - without this, the step
    // strip would show all 5 steps (PluginView's own construction-time
    // default) until the user's first nav switch called setStepStripMode.
    pluginView_->setStepStripMode(Views::PluginView::StepStripMode::Creation);

    // Environment groups diagnostics and infrastructure utilities so the main rail
    // stays short. Put Doctor first so setup checks are the most discoverable tool.
    // Validator is intentionally not nested here - it's a standalone offline-analysis
    // workflow (paste & check, or reached from Workspace's "Check Offline" button),
    // not part of "is my connected environment healthy" like Doctor/Docker/Infra.
    auto* environmentTabs = new QTabWidget(this);
    environmentTabs->setObjectName("EnvironmentTabs");
    environmentTabs->addTab(new Views::DoctorView(doctorVm_, this), tr("Doctor"));    // 0
    environmentTabs->addTab(new Views::DockerView(dockerVm_, this), tr("Docker"));    // 1
    environmentTabs->addTab(new Views::InfraView(infraVm_, this),   tr("Infra"));     // 2
    constexpr int kEnvDoctorTab = 0;
    constexpr int kEnvDockerTab = 1;

    // Remember the last-viewed sub-tab across sessions, and refresh the
    // container list automatically whenever the Docker sub-tab becomes
    // active (rather than requiring a manual Refresh click every time).
    connect(environmentTabs, &QTabWidget::currentChanged, this,
            [this, kEnvDockerTab](int index) {
                QSettings().setValue("Environment/lastTab", index);
                if (index == kEnvDockerTab)
                    dockerVm_.refresh();
            });
    QSettings environmentSettings;
    environmentTabs->setCurrentIndex(
        environmentSettings.value("Environment/lastTab", kEnvDoctorTab).toInt());

    // ── Navigation helpers ──────────────────────────────────────────────────
    auto goTo = [this](int row) {
        // Programmatic navigation (e.g. Home's quick actions) to a row hidden by
        // the current mode transparently switches mode first, rather than landing
        // on a page with no visible nav highlight.
        if (!rowVisibleInMode(row, appMode_) && navRail_->item(row))
            setAppMode(appMode_ == AppMode::PluginCreation
                ? AppMode::ConnectOperate : AppMode::PluginCreation);
        navRail_->setCurrentRow(row);   // also switches the stack via the signal
        contentStack_->setCurrentIndex(rowToStackPage_[row]);
    };

    auto toValidator = [this, goTo, validatorView](const QString& content) {
        goTo(NavValidator);
        validatorView->checkContent(content);
    };
    connect(pluginView, &Views::PluginView::openInValidatorRequested, this, toValidator);

    // ── Home (Dashboard) quick actions ──────────────────────────────────────
    connect(dashboardView, &Views::DashboardView::connectRequested,
            this, &MainWindow::showConnectionDialog);
    connect(dashboardView, &Views::DashboardView::runDoctorRequested,
            this, [this, goTo, environmentTabs, kEnvDoctorTab] {
                if (!settingsVm_.isConnected()) {
                    showConnectionDialog();
                    if (!settingsVm_.isConnected())
                        return;
                }
                goTo(NavEnvironment);
                environmentTabs->setCurrentIndex(kEnvDoctorTab);
                doctorVm_.runAllChecks();
            });
    connect(dashboardView, &Views::DashboardView::openWorkspaceRequested,
            this, [goTo] { goTo(NavWorkspace); });
    connect(dashboardView, &Views::DashboardView::openCheckBuildRequested,
            this, [goTo] { goTo(NavCheckBuild); });
    connect(dashboardView, &Views::DashboardView::openValidatorRequested,
            this, [goTo] { goTo(NavValidator); });
    connect(dashboardView, &Views::DashboardView::openPacketToolsRequested,
            this, [goTo] { goTo(NavPacketTools); });
    connect(dashboardView, &Views::DashboardView::openLogsRequested,
            this, [goTo] { goTo(NavLogs); });

    // Keep this order aligned with NavIndex and the navRail_ addItem calls in setupNavigation(),
    // for the rows that map 1:1 onto their own page (NavCheckBuild is the one exception -
    // it shares NavWorkspace's page, handled via rowToStackPage_ below).
    contentStack_->addWidget(dashboardView);                                    // 0 Home
    contentStack_->addWidget(pluginView);                                       // 1 Workspace (plugins + CMD/TLM)
    contentStack_->addWidget(new Views::SettingsView(settingsVm_, this));       // 2 Settings
    contentStack_->addWidget(environmentTabs);                                  // 3 Environment
    contentStack_->addWidget(validatorView);                                    // 4 Validator
    contentStack_->addWidget(new Views::PacketToolsView(packetToolsVm_, this)); // 5 Packets
    contentStack_->addWidget(new Views::LogViewerView(logViewerVm_, this));     // 6 Logs

    rowToStackPage_ = { NavHome, NavWorkspace, NavSettings, NavEnvironment,
                        NavValidator, NavPacketTools, NavLogs,
                        /* NavCheckBuild shares Workspace's page */ NavWorkspace };
}

void MainWindow::showConnectionDialog()
{
    Dialogs::ConnectionDialog dlg(settingsVm_, this);
    dlg.exec();
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu   = menuBar()->addMenu(tr("&File"));
    auto* exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);

    auto* viewMenu      = menuBar()->addMenu(tr("&View"));
    auto* refreshAction = new QAction(tr("&Refresh"), this);
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

    viewMenu->addSeparator();
    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    pluginCreationAction_ = new QAction(tr("Plugin Creation Mode"), this);
    pluginCreationAction_->setCheckable(true);
    pluginCreationAction_->setChecked(appMode_ == AppMode::PluginCreation);
    modeGroup->addAction(pluginCreationAction_);
    viewMenu->addAction(pluginCreationAction_);
    connect(pluginCreationAction_, &QAction::triggered, this, [this] {
        setAppMode(AppMode::PluginCreation);
    });

    connectOperateAction_ = new QAction(tr("Connect && Operate Mode"), this);
    connectOperateAction_->setCheckable(true);
    connectOperateAction_->setChecked(appMode_ == AppMode::ConnectOperate);
    modeGroup->addAction(connectOperateAction_);
    viewMenu->addAction(connectOperateAction_);
    connect(connectOperateAction_, &QAction::triggered, this, [this] {
        setAppMode(AppMode::ConnectOperate);
    });

    // Language: takes effect on next launch - this app has no dynamic
    // retranslation wiring (QEvent::LanguageChange handlers on every view),
    // so switching live would leave already-constructed widgets showing
    // stale text. Persisted via the same plain QSettings store as window
    // geometry/state, not the JSON-backed SettingsService (this is app
    // chrome, not a COSMOS connection setting).
    viewMenu->addSeparator();
    auto* languageMenu = viewMenu->addMenu(tr("&Language"));
    auto* languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);

    QSettings languageSettings;
    const QString currentLanguage = languageSettings.value("App/language", "en").toString();

    auto* englishAction = new QAction("English", this);
    englishAction->setCheckable(true);
    englishAction->setChecked(currentLanguage != "ko");
    languageGroup->addAction(englishAction);
    languageMenu->addAction(englishAction);

    auto* koreanAction = new QAction(QString::fromUtf8("한국어"), this);
    koreanAction->setCheckable(true);
    koreanAction->setChecked(currentLanguage == "ko");
    languageGroup->addAction(koreanAction);
    languageMenu->addAction(koreanAction);

    auto applyLanguageChoice = [this](const QString& code) {
        QSettings s;
        if (s.value("App/language", "en").toString() == code)
            return;
        s.setValue("App/language", code);
        QMessageBox::information(this, tr("Restart Required"),
            tr("The language change will take effect the next time you start the application."));
    };
    connect(englishAction, &QAction::triggered, this, [applyLanguageChoice] { applyLanguageChoice("en"); });
    connect(koreanAction,  &QAction::triggered, this, [applyLanguageChoice] { applyLanguageChoice("ko"); });

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));

    auto* guideAction = new QAction(tr("&User Guide"), this);
    guideAction->setShortcut(QKeySequence::HelpContents);
    connect(guideAction, &QAction::triggered, this, [this] {
        auto* dlg = new Dialogs::UserGuideDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    helpMenu->addAction(guideAction);

    helpMenu->addSeparator();

    auto* aboutAction = new QAction(tr("&About"), this);
    connect(aboutAction, &QAction::triggered, this, [this] {
        Dialogs::AboutDialog dlg(this);
        dlg.exec();
    });
    helpMenu->addAction(aboutAction);
}

void MainWindow::setupStatusBar()
{
    // A flat button (not a label) so the connection status doubles as a
    // Connect entry point that works from any view.
    connectionButton_ = new QPushButton("  " + tr("Disconnected") + "  ", this);
    connectionButton_->setFlat(true);
    connectionButton_->setCursor(Qt::PointingHandCursor);
    connectionButton_->setToolTip(tr("Click to open the connection dialog"));
    connect(connectionButton_, &QPushButton::clicked,
            this, &MainWindow::showConnectionDialog);

    dockerLabel_ = new QLabel("  " + tr("Docker: --") + "  ", this);

    statusBar()->addWidget(connectionButton_);
    statusBar()->addWidget(dockerLabel_);
    statusBar()->setObjectName("statusBar");
}

void MainWindow::connectSignals()
{
    connect(&dashboardVm_, &ViewModels::DashboardViewModel::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(&dashboardVm_, &ViewModels::DashboardViewModel::dockerStatusChanged,
            this, [this] {
                dockerLabel_->setText("  " + tr("Docker:") + " " + dashboardVm_.dockerStatus() + "  ");
            });

    onConnectionStatusChanged();
    dockerLabel_->setText("  " + tr("Docker:") + " " + dashboardVm_.dockerStatus() + "  ");
}

void MainWindow::onNavItemSelected(int index)
{
    if (index >= 0 && index < rowToStackPage_.size())
        contentStack_->setCurrentIndex(rowToStackPage_[index]);

    // Workspace and Check & Build share one PluginView page but show
    // different step ranges - sync whenever navigation lands on either one.
    // All navigation paths (nav-rail click, goTo(), Ctrl+<n> shortcuts, the
    // mode toggle) funnel through here via navRail_'s currentRowChanged.
    if (pluginView_) {
        if (index == NavWorkspace)
            pluginView_->setStepStripMode(Views::PluginView::StepStripMode::Creation);
        else if (index == NavCheckBuild)
            pluginView_->setStepStripMode(Views::PluginView::StepStripMode::CheckBuild);
    }
}

void MainWindow::onConnectionStatusChanged()
{
    const QString status = dashboardVm_.connectionStatus();
    connectionButton_->setText("  " + status + "  ");
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings windowSettings;
    windowSettings.setValue("MainWindow/geometry", saveGeometry());
    windowSettings.setValue("MainWindow/state", saveState());

    logViewerVm_.stopStream();
    settingsVm_.disconnect();
    event->accept();
}

} // namespace OpenC3::UI
