#include "DashboardView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
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
    auto* title = new QLabel("Dashboard", this);
    QFont tf = title->font();
    tf.setPointSize(18);
    tf.setBold(true);
    title->setFont(tf);
    title->setObjectName("PageTitle");
    root->addWidget(title);

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
    // Connection status
    connect(&vm_, &ViewModels::DashboardViewModel::connectionStatusChanged,
            this, [this] {
                const QString s = vm_.connectionStatus();
                const auto style = s.startsWith("Connected")
                    ? Widgets::BadgeStyle::Success
                    : s.startsWith("Error")
                        ? Widgets::BadgeStyle::Error
                        : Widgets::BadgeStyle::Neutral;
                connectionBadge_->setStyle(style, s);
            });

    // Docker status
    connect(&vm_, &ViewModels::DashboardViewModel::dockerStatusChanged,
            this, [this] {
                const QString s = vm_.dockerStatus();
                const auto style = s.startsWith("Running")
                    ? Widgets::BadgeStyle::Success
                    : Widgets::BadgeStyle::Error;
                dockerBadge_->setStyle(style, s);
                containerLabel_->setText(
                    QString::number(vm_.containerCount()));
            });

    // Version
    connect(&vm_, &ViewModels::DashboardViewModel::openC3VersionChanged,
            this, [this] {
                versionLabel_->setText(vm_.openC3Version());
            });

    // Metrics
    connect(&vm_, &ViewModels::DashboardViewModel::metricsChanged,
            this, [this] {
                cpuCard_->setValue(vm_.cpuPercent());
                memCard_->setValue(vm_.memPercent());
                diskCard_->setValue(vm_.diskPercent());
            });
}

} // namespace OpenC3::UI::Views
