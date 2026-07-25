#include "InstalledPluginsPanel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace OpenC3::UI::Widgets {

InstalledPluginsPanel::InstalledPluginsPanel(
    ViewModels::PluginViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    refreshBtn_ = new QPushButton(tr("Refresh Installed"), this);
    removeBtn_  = new QPushButton(tr("Uninstall Instance"), this);
    removeBtn_->setObjectName("DangerButton");
    removeBtn_->setEnabled(false);
    removeBtn_->setToolTip(tr(
        "Uninstall the selected COSMOS instance using its exact __<counter> id."));

    jobStatusLabel_ = new QLabel(tr("No active plugin job."), this);
    jobStatusLabel_->setObjectName("PluginJobStatus");
    jobStatusLabel_->setWordWrap(true);
    jobStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    tableView_ = new QTableView(this);
    tableView_->setObjectName("InstalledPluginTable");
    tableView_->setModel(vm_.installedPluginModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->horizontalHeader()->setSectionResizeMode(
        ViewModels::InstalledPluginTableModel::Instance, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(
        ViewModels::InstalledPluginTableModel::Gem, QHeaderView::ResizeToContents);
    tableView_->setMinimumHeight(150);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(refreshBtn_);
    toolbar->addWidget(removeBtn_);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(jobStatusLabel_);
    layout->addWidget(tableView_);

    connect(refreshBtn_, &QPushButton::clicked,
            &vm_, &ViewModels::PluginViewModel::refreshInstalled);
    connect(removeBtn_, &QPushButton::clicked,
            this, &InstalledPluginsPanel::onRemoveClicked);
    connect(tableView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &InstalledPluginsPanel::onSelectionOrBusyChanged);
    connect(&vm_, &ViewModels::PluginViewModel::busyChanged,
            this, &InstalledPluginsPanel::onSelectionOrBusyChanged);

    connect(&vm_, &ViewModels::PluginViewModel::jobStatusChanged,
            this, [this](const QString& processName, const QString& state,
                         const QString& detail, bool finished, bool successful) {
                QString heading;
                if (!finished)
                    heading = tr("Plugin job in progress");
                else if (successful)
                    heading = tr("Plugin job finished");
                else
                    heading = tr("Plugin job failed");

                QString text = tr("%1: %2\nProcess: %3")
                                   .arg(heading, state, processName);
                if (!detail.trimmed().isEmpty()) text += "\n" + detail.trimmed();
                jobStatusLabel_->setText(text);
                jobStatusLabel_->setToolTip(text);
            });
}

void InstalledPluginsPanel::onSelectionOrBusyChanged()
{
    removeBtn_->setEnabled(
        !vm_.isBusy() && tableView_->selectionModel()->hasSelection());
    refreshBtn_->setEnabled(!vm_.isBusy());
}

void InstalledPluginsPanel::onRemoveClicked()
{
    const auto rows = tableView_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;

    const auto* plugin = vm_.installedPluginModel()->pluginAt(rows.first().row());
    if (!plugin) return;
    const QString instanceId = QString::fromStdString(plugin->name);

    if (QMessageBox::question(this, tr("Confirm Uninstall"),
            tr("Start uninstalling this exact COSMOS plugin instance?\n\n%1")
                .arg(instanceId),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        vm_.remove(instanceId);
    }
}

} // namespace OpenC3::UI::Widgets
