#include "MainWindow.h"
#include "views/DashboardView.h"
#include "views/DockerView.h"
#include "views/DoctorView.h"
#include "views/SettingsView.h"
#include "views/PluginView.h"
#include "dialogs/AboutDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QCloseEvent>
#include <QApplication>
#include <QIcon>
#include <QFont>

namespace OpenC3::UI {

MainWindow::MainWindow(
    ViewModels::DashboardViewModel& dashboard,
    ViewModels::DockerViewModel&    docker,
    ViewModels::DoctorViewModel&    doctor,
    ViewModels::SettingsViewModel&  settings,
    QWidget*                        parent)
    : QMainWindow(parent)
    , dashboardVm_(dashboard)
    , dockerVm_(docker)
    , doctorVm_(doctor)
    , settingsVm_(settings)
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

    // ── Navigation rail ───────────────────────────────────────────────────────
    setupNavigation();

    // ── Content area ──────────────────────────────────────────────────────────
    contentStack_ = new QStackedWidget(this);
    setupViews();

    // ── Splitter ──────────────────────────────────────────────────────────────
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
    addItem("⚙️",  "Settings");

    navRail_->setCurrentRow(0);
    navRail_->setObjectName("navRail");

    connect(navRail_, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavItemSelected);
}

void MainWindow::setupViews()
{
    contentStack_->addWidget(new Views::DashboardView(dashboardVm_, this));
    contentStack_->addWidget(new Views::DockerView(dockerVm_, this));
    contentStack_->addWidget(new Views::DoctorView(doctorVm_, this));
    contentStack_->addWidget(new Views::PluginView(this));
    contentStack_->addWidget(new Views::SettingsView(settingsVm_, this));
}

void MainWindow::setupMenuBar()
{
    // ── File ──────────────────────────────────────────────────────────────────
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);

    // ── View ──────────────────────────────────────────────────────────────────
    auto* viewMenu = menuBar()->addMenu("&View");
    auto* refreshAction = new QAction("&Refresh", this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    viewMenu->addAction(refreshAction);

    // ── Help ──────────────────────────────────────────────────────────────────
    auto* helpMenu = menuBar()->addMenu("&Help");
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
                dockerLabel_->setText(
                    "  🐳 " + dashboardVm_.dockerStatus() + "  ");
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
    settingsVm_.disconnect();
    event->accept();
}

} // namespace OpenC3::UI
