#include "DashboardView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QStyle>

#include <cstddef>

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
    auto* pageTitle = new QLabel(tr("Home"), this);
    QFont tf = pageTitle->font();
    tf.setPointSize(18);
    tf.setBold(true);
    pageTitle->setFont(tf);
    pageTitle->setObjectName("PageTitle");
    root->addWidget(pageTitle);

    // ── Guidance line — tells a first-time user what to do next ────────────────
    guidanceLabel_ = new QLabel(this);
    guidanceLabel_->setObjectName("SubLabel");
    guidanceLabel_->setWordWrap(true);
    root->addWidget(guidanceLabel_);

    auto* recommendedRow = new QHBoxLayout;
    recommendedActionBtn_ = new QPushButton(this);
    recommendedActionBtn_->setObjectName("PrimaryButton");
    recommendedActionBtn_->setMinimumWidth(220);
    connect(recommendedActionBtn_, &QPushButton::clicked,
            this, &DashboardView::onRecommendedActionClicked);
    recommendedRow->addWidget(recommendedActionBtn_);
    recommendedRow->addStretch();
    root->addLayout(recommendedRow);

    // ── Quick actions ──────────────────────────────────────────────────────────
    auto* actionsGroup  = new QGroupBox(tr("Quick Actions"), this);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);
    actionsLayout->setSpacing(12);

    auto* stepLayout = new QHBoxLayout;
    stepLayout->setSpacing(12);

    auto addStepCard = [&](std::size_t index,
                           const QString& step,
                           const QString& cardTitle,
                           const QString& description,
                           auto signal) {
        auto* card = new QFrame(actionsGroup);
        card->setObjectName("StepCard");
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setMinimumWidth(190);

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(8);

        auto* stepLabel = new QLabel(step, card);
        stepLabel->setObjectName("StepEyebrow");

        auto* titleLabel = new QLabel(cardTitle, card);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setObjectName("StepTitle");

        auto* descriptionLabel = new QLabel(description, card);
        descriptionLabel->setObjectName("SubLabel");
        descriptionLabel->setWordWrap(true);

        stepBadges_[index] = new Widgets::StatusBadge(
            tr("Pending"), Widgets::BadgeStyle::Neutral, card);
        stepButtons_[index] = new QPushButton(cardTitle, card);
        connect(stepButtons_[index], &QPushButton::clicked, this, signal);

        cardLayout->addWidget(stepLabel);
        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(descriptionLabel);
        cardLayout->addStretch();
        cardLayout->addWidget(stepBadges_[index], 0, Qt::AlignLeft);
        cardLayout->addWidget(stepButtons_[index]);
        stepLayout->addWidget(card);
    };

    addStepCard(0, tr("Step 1"), tr("Connect to OpenC3"),
                tr("Choose a profile and verify the toolkit can reach OpenC3."),
                &DashboardView::connectRequested);
    addStepCard(1, tr("Step 2"), tr("Run Doctor"),
                tr("Check Docker, containers, and the local OpenC3 environment."),
                &DashboardView::runDoctorRequested);
    addStepCard(2, tr("Step 3"), tr("Open Workspace"),
                tr("Manage plugins and start editing once the environment is ready."),
                &DashboardView::openWorkspaceRequested);
    stepLayout->addStretch();
    actionsLayout->addLayout(stepLayout);

    auto* toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(8);
    auto* toolsLabel = new QLabel(tr("More tools:"), actionsGroup);
    toolsLabel->setObjectName("SubLabel");
    toolsRow->addWidget(toolsLabel);

    auto addToolLink = [&](const QString& text, auto signal) {
        auto* btn = new QPushButton(text, actionsGroup);
        btn->setObjectName("LinkButton");
        connect(btn, &QPushButton::clicked, this, signal);
        toolsRow->addWidget(btn);
    };

    addToolLink(tr("Validator"),    &DashboardView::openValidatorRequested);
    addToolLink(tr("Packet Tools"), &DashboardView::openPacketToolsRequested);
    addToolLink(tr("Logs"),         &DashboardView::openLogsRequested);
    toolsRow->addStretch();
    actionsLayout->addLayout(toolsRow);

    root->addWidget(actionsGroup);

    // ── Status row ────────────────────────────────────────────────────────────
    auto* statusGroup  = new QGroupBox(tr("System Status"), this);
    auto* statusLayout = new QGridLayout(statusGroup);
    statusLayout->setColumnStretch(1, 1);
    statusLayout->setSpacing(10);

    auto addStatusRow = [&](int row, const QString& label, QWidget* badge) {
        statusLayout->addWidget(new QLabel(label, statusGroup), row, 0);
        statusLayout->addWidget(badge, row, 1, Qt::AlignLeft);
    };

    connectionBadge_ = new Widgets::StatusBadge(tr("Disconnected"),
                            Widgets::BadgeStyle::Neutral, this);
    dockerBadge_     = new Widgets::StatusBadge(tr("Unknown"),
                            Widgets::BadgeStyle::Neutral, this);
    versionLabel_    = new QLabel("--", this);
    containerLabel_  = new QLabel("0", this);

    addStatusRow(0, tr("Connection:"), connectionBadge_);
    addStatusRow(1, tr("Docker:"),     dockerBadge_);
    addStatusRow(2, tr("OpenC3 Version:"), versionLabel_);
    addStatusRow(3, tr("Running Containers:"), containerLabel_);

    root->addWidget(statusGroup);

    // ── Metric cards ──────────────────────────────────────────────────────────
    auto* metricsGroup  = new QGroupBox(tr("Resource Usage"), this);
    auto* metricsLayout = new QHBoxLayout(metricsGroup);
    metricsLayout->setSpacing(12);

    cpuCard_  = new Widgets::MetricCard(tr("CPU"),    "%", metricsGroup);
    memCard_  = new Widgets::MetricCard(tr("Memory"), "%", metricsGroup);
    diskCard_ = new Widgets::MetricCard(tr("Disk"),   "%", metricsGroup);

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

    const std::size_t nextStep = !connected ? 0U : (!dockerRunning ? 1U : 2U);

    for (std::size_t i = 0; i < stepButtons_.size(); ++i) {
        const bool complete = (i == 0U && connected)
            || (i == 1U && connected && dockerRunning);
        const bool current = i == nextStep;

        stepBadges_[i]->setStyle(
            complete ? Widgets::BadgeStyle::Success
                     : current ? Widgets::BadgeStyle::Info
                               : Widgets::BadgeStyle::Neutral,
            complete ? tr("Success") : current ? tr("Next") : tr("Pending"));

        stepButtons_[i]->setObjectName(current ? "PrimaryButton" : "");
        stepButtons_[i]->style()->unpolish(stepButtons_[i]);
        stepButtons_[i]->style()->polish(stepButtons_[i]);
        stepButtons_[i]->setEnabled(!complete || i == 2U);
    }

    if (!connected) {
        guidanceLabel_->setText(tr(
            "Start by connecting to an OpenC3 environment. After connection, "
            "run Doctor to verify Docker before opening the Workspace."));
        recommendedActionBtn_->setText(tr("Connect to OpenC3"));
        return;
    }

    if (!dockerRunning) {
        guidanceLabel_->setText(tr(
            "Connection is ready. Run Doctor next to check Docker, containers, "
            "and OpenC3 before editing plugins."));
        recommendedActionBtn_->setText(tr("Run Doctor"));
        return;
    }

    guidanceLabel_->setText(tr(
        "Environment checks look good. Open the Workspace to manage plugins "
        "and edit CMD/TLM files; Validator, Packet Tools, and Logs are "
        "available under More tools."));
    recommendedActionBtn_->setText(tr("Open Workspace"));
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
