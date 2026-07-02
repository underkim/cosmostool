#include "DashboardView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFont>

namespace OpenC3::UI::Views {

DashboardView::DashboardView(
    ViewModels::DashboardViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void DashboardView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    // ── Page title ────────────────────────────────────────────────────────────
    auto* title = new QLabel("Home", this);
    QFont tf = title->font();
    tf.setPointSize(18);
    tf.setBold(true);
    title->setFont(tf);
    title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Guidance line — tells a first-time user what to do next ────────────────
    guidanceLabel_ = new QLabel(this);
    guidanceLabel_->setObjectName("SubLabel");
    guidanceLabel_->setWordWrap(true);
    root->addWidget(guidanceLabel_);

    auto* recommendedRow = new QHBoxLayout;
    recommendedActionBtn_ = new QPushButton(this);
    recommendedActionBtn_->setObjectName("PrimaryButton");
    recommendedActionBtn_->setMinimumWidth(180);
    connect(recommendedActionBtn_, &QPushButton::clicked,
            this, &DashboardView::onRecommendedActionClicked);
    recommendedRow->addWidget(recommendedActionBtn_);
    recommendedRow->addStretch();
    root->addLayout(recommendedRow);

    // ── Quick actions ──────────────────────────────────────────────────────────
    auto* actionsGroup  = new QGroupBox("Quick Actions", this);
    auto* actionsLayout = new QHBoxLayout(actionsGroup);
    actionsLayout->setSpacing(8);

    auto addAction = [&](const QString& text, bool primary, auto signal) {
        auto* btn = new QPushButton(text, actionsGroup);
        if (primary) btn->setObjectName("PrimaryButton");
        connect(btn, &QPushButton::clicked, this, signal);
        actionsLayout->addWidget(btn);
        return btn;
    };

    addAction("Run Doctor",   true,  &DashboardView::runDoctorRequested);
    addAction("Connect",      false, &DashboardView::connectRequested);
    addAction("Workspace",    false, &DashboardView::openWorkspaceRequested);
    addAction("CMD / TLM",    false, &DashboardView::openCmdTlmRequested);
    addAction("Validator",    false, &DashboardView::openValidatorRequested);
    addAction("Packet Tools", false, &DashboardView::openPacketToolsRequested);
    addAction("Logs",         false, &DashboardView::openLogsRequested);
    actionsLayout->addStretch();

    root->addWidget(actionsGroup);

    // ── Status row ────────────────────────────────────────────────────────────
    auto* statusGroup  = new QGroupBox("System Status", this);
    auto* statusLayout = new QGridLayout(statusGroup);
    statusLayout->setColumnStretch(1, 1);
    statusLayout->setSpacing(10);

    auto addStatusRow = [&](int row, const QString& label, QWidget* badge) {
        statusLayout->addWidget(new QLabel(label, statusGroup), row, 0);
        statusLayout->addWidget(badge, row, 1, Qt::AlignLeft);
    };

    connectionBadge_ = new Widgets::StatusBadge("Disconnected",
                            Widgets::BadgeStyle::Neutral, this);
    dockerBadge_     = new Widgets::StatusBadge("Unknown",
                            Widgets::BadgeStyle::Neutral, this);
    versionLabel_    = new QLabel("--", this);
    containerLabel_  = new QLabel("0", this);

    addStatusRow(0, "Connection:", connectionBadge_);
    addStatusRow(1, "Docker:",     dockerBadge_);
    addStatusRow(2, "OpenC3 Version:", versionLabel_);
    addStatusRow(3, "Running Containers:", containerLabel_);

    root->addWidget(statusGroup);

    // ── Metric cards ──────────────────────────────────────────────────────────
    auto* metricsGroup  = new QGroupBox("Resource Usage", this);
    auto* metricsLayout = new QHBoxLayout(metricsGroup);
    metricsLayout->setSpacing(12);

    cpuCard_  = new Widgets::MetricCard("CPU",    "%", metricsGroup);
    memCard_  = new Widgets::MetricCard("Memory", "%", metricsGroup);
    diskCard_ = new Widgets::MetricCard("Disk",   "%", metricsGroup);

    metricsLayout->addWidget(cpuCard_);
    metricsLayout->addWidget(memCard_);
    metricsLayout->addWidget(diskCard_);
    metricsLayout->addStretch();

    root->addWidget(metricsGroup);
    root->addStretch();
}

void DashboardView::bindViewModel()
{
    auto updateConnection = [this] {
        const QString s = vm_.connectionStatus();
        const auto style = s.startsWith("Connected")
            ? Widgets::BadgeStyle::Success
            : s.startsWith("Error")
                ? Widgets::BadgeStyle::Error
                : Widgets::BadgeStyle::Neutral;
        connectionBadge_->setStyle(style, s);

        // Guide the user toward the right next step based on connection state.
        if (s.startsWith("Connected")) {
            guidanceLabel_->setText(
                "Connected. Open the Workspace to manage plugins, or jump to "
                "CMD / TLM, Validator, Packet Tools, or Logs.");
        } else {
            guidanceLabel_->setText(
                "Not connected. Click Connect to choose a profile — or create a "
                "WSL profile in one step — then run Doctor to check your setup.");
        }
        updateHomeGuidance();
    };

    auto updateDocker = [this] {
        const QString s = vm_.dockerStatus();
        const auto style = s.startsWith("Running")
            ? Widgets::BadgeStyle::Success
            : Widgets::BadgeStyle::Error;
        dockerBadge_->setStyle(style, s);
        containerLabel_->setText(QString::number(vm_.containerCount()));
        updateHomeGuidance();
    };

    auto updateVersion = [this] {
        versionLabel_->setText(vm_.openC3Version());
    };

    auto updateMetrics = [this] {
        cpuCard_->setValue(vm_.cpuPercent());
        memCard_->setValue(vm_.memPercent());
        diskCard_->setValue(vm_.diskPercent());
    };

    connect(&vm_, &ViewModels::DashboardViewModel::connectionStatusChanged,
            this, updateConnection);

    connect(&vm_, &ViewModels::DashboardViewModel::dockerStatusChanged,
            this, updateDocker);

    connect(&vm_, &ViewModels::DashboardViewModel::openC3VersionChanged,
            this, updateVersion);

    connect(&vm_, &ViewModels::DashboardViewModel::metricsChanged,
            this, updateMetrics);

    updateConnection();
    updateDocker();
    updateVersion();
    updateMetrics();
}

void DashboardView::updateHomeGuidance()
{
    const QString connection = vm_.connectionStatus();
    const QString docker = vm_.dockerStatus();
    const bool connected = connection.startsWith("Connected");
    const bool dockerRunning = docker.startsWith("Running");

    if (!connected) {
        guidanceLabel_->setText(
            "Connect to an OpenC3 environment to enable workspace tools.");
        recommendedActionBtn_->setText("Connect");
        return;
    }

    if (!dockerRunning) {
        guidanceLabel_->setText(
            "Connected. Run Doctor to check Docker and OpenC3 before editing.");
        recommendedActionBtn_->setText("Run Doctor");
        return;
    }

    guidanceLabel_->setText(
        "Ready. Run Doctor any time for diagnostics, or jump into CMD / TLM, "
        "Validator, Packet Tools, or Logs.");
    recommendedActionBtn_->setText("Open Workspace");
}

void DashboardView::onRecommendedActionClicked()
{
    const bool connected = vm_.connectionStatus().startsWith("Connected");
    const bool dockerRunning = vm_.dockerStatus().startsWith("Running");

    if (!connected) {
        emit connectRequested();
    } else if (!dockerRunning) {
        emit runDoctorRequested();
    } else {
        emit openWorkspaceRequested();
    }
}

} // namespace OpenC3::UI::Views
