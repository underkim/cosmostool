#include "MainWindow.h"
#include "views/DashboardView.h"
#include "views/DockerView.h"
#include "views/DoctorView.h"
#include "views/SettingsView.h"
#include "views/PluginView.h"
#include "views/CmdTlmView.h"
#include "views/PacketToolsView.h"
#include "views/LogViewerView.h"
#include "dialogs/AboutDialog.h"
#include "dialogs/UserGuideDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QCloseEvent>
#include <QApplication>
#include <QFont>

namespace OpenC3::UI {

MainWindow::MainWindow(
    ViewModels::DashboardViewModel&    dashboard,
    ViewModels::DockerViewModel&       docker,
    ViewModels::DoctorViewModel&       doctor,
    ViewModels::SettingsViewModel&     settings,
    ViewModels::PluginViewModel&       plugin,
    ViewModels::CmdTlmViewModel&       cmdTlm,
    ViewModels::PacketToolsViewModel&  packetTools,
    ViewModels::LogViewerViewModel&    logViewer,
    QWidget*                           parent)
    : QMainWindow(parent)
    , dashboardVm_(dashboard)
    , dockerVm_(docker)
    , doctorVm_(doctor)
    , settingsVm_(settings)
    , pluginVm_(plugin)
    , cmdTlmVm_(cmdTlm)
    , packetToolsVm_(packetTools)
    , logViewerVm_(logViewer)
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

    auto addItem = [&](const QString& icon, const QString& text) {
        auto* item = new QListWidgetItem(icon + "  " + text);
        item->setSizeHint(QSize(200, 40));
        QFont f = item->font();
        f.setPointSize(11);
        item->setFont(f);
        navRail_->addItem(item);
    };

    addItem("📊", "Dashboard");
    addItem("🐳", "Docker");
    addItem("🩺", "Doctor");
    addItem("🧩", "Plugins");
    addItem("📝", "CMD / TLM");
    addItem("📦", "Packet Tools");
    addItem("📋", "Log Viewer");
    addItem("⚙️",  "Settings");

    navRail_->setCurrentRow(0);
    navRail_->setObjectName("navRail");

    connect(navRail_, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavItemSelected);
}

void MainWindow::setupViews()
{
    contentStack_->addWidget(new Views::DashboardView(dashboardVm_,   this));  // 0
    contentStack_->addWidget(new Views::DockerView(dockerVm_,         this));  // 1
    contentStack_->addWidget(new Views::DoctorView(doctorVm_,         this));  // 2
    contentStack_->addWidget(new Views::PluginView(pluginVm_,         this));  // 3
    contentStack_->addWidget(new Views::CmdTlmView(cmdTlmVm_,         this));  // 4
    contentStack_->addWidget(new Views::PacketToolsView(packetToolsVm_,this)); // 5
    contentStack_->addWidget(new Views::LogViewerView(logViewerVm_,   this));  // 6
    contentStack_->addWidget(new Views::SettingsView(settingsVm_,     this));  // 7
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
        if (idx == 0) dashboardVm_.refresh();
        else if (idx == 1) dockerVm_.refresh();
        else if (idx == 3) pluginVm_.refresh();
    });
    viewMenu->addAction(refreshAction);

    auto* helpMenu       = menuBar()->addMenu("&Help");

    auto* guideAction = new QAction("&User Guide", this);
    guideAction->setShortcut(QKeySequence::HelpContents); // F1
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
    connectionLabel_ = new QLabel("  ⚪ Disconnected  ", this);
    dockerLabel_     = new QLabel("  🐳 Docker: --  ", this);

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
                dockerLabel_->setText("  🐳 " + dashboardVm_.dockerStatus() + "  ");
            });
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onNavItemSelected(int index)
{
    contentStack_->setCurrentIndex(index);
}

void MainWindow::onConnectionStatusChanged()
{
    const QString status = dashboardVm_.connectionStatus();
    const QString icon   = status.startsWith("Connected") ? "🟢" : "🔴";
    connectionLabel_->setText("  " + icon + " " + status + "  ");
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    logViewerVm_.stopStream();
    settingsVm_.disconnect();
    event->accept();
}

} // namespace OpenC3::UI
