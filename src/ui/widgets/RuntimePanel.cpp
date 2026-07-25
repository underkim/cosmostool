#include "RuntimePanel.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace OpenC3::UI::Widgets {

RuntimePanel::RuntimePanel(ViewModels::RuntimeViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    refreshTargetsBtn_ = new QPushButton(tr("Refresh Targets"), this);
    refreshTargetsBtn_->setToolTip(tr(
        "Lists COSMOS targets actually known to the connected scope "
        "(TargetModel) - requires an installed plugin, not a Workspace "
        "source folder."));

    targetList_ = new QListView(this);
    targetList_->setObjectName("CosmosTargetList");
    targetList_->setModel(vm_.targetModel());
    targetList_->setSelectionMode(QAbstractItemView::SingleSelection);
    targetList_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    targetList_->setMaximumWidth(220);

    packetKindCombo_ = new QComboBox(this);
    packetKindCombo_->addItem(tr("Telemetry"));
    packetKindCombo_->addItem(tr("Commands"));

    packetList_ = new QListView(this);
    packetList_->setObjectName("CosmosPacketList");
    packetList_->setModel(vm_.packetModel());
    packetList_->setSelectionMode(QAbstractItemView::SingleSelection);
    packetList_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    packetList_->setMaximumWidth(220);

    telemetryTable_ = new QTableView(this);
    telemetryTable_->setObjectName("CosmosTelemetryTable");
    telemetryTable_->setModel(vm_.telemetryModel());
    telemetryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    telemetryTable_->horizontalHeader()->setStretchLastSection(true);

    statusLabel_ = new QLabel(tr("Not loaded."), this);
    statusLabel_->setObjectName("RuntimeStatus");
    statusLabel_->setWordWrap(true);

    auto* targetColumn = new QVBoxLayout;
    targetColumn->addWidget(new QLabel(tr("Targets"), this));
    targetColumn->addWidget(refreshTargetsBtn_);
    targetColumn->addWidget(targetList_, 1);

    auto* packetColumn = new QVBoxLayout;
    packetColumn->addWidget(new QLabel(tr("Packets"), this));
    packetColumn->addWidget(packetKindCombo_);
    packetColumn->addWidget(packetList_, 1);

    auto* valueColumn = new QVBoxLayout;
    valueColumn->addWidget(new QLabel(tr("Current Telemetry Values"), this));
    valueColumn->addWidget(telemetryTable_, 1);

    auto* columns = new QHBoxLayout;
    columns->addLayout(targetColumn);
    columns->addLayout(packetColumn);
    columns->addLayout(valueColumn, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(statusLabel_);
    layout->addLayout(columns, 1);

    connect(refreshTargetsBtn_, &QPushButton::clicked,
            &vm_, &ViewModels::RuntimeViewModel::refreshTargets);
    connect(targetList_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RuntimePanel::onTargetSelectionChanged);
    connect(packetKindCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RuntimePanel::onPacketKindChanged);
    connect(packetList_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RuntimePanel::onPacketSelectionChanged);
    connect(&vm_, &ViewModels::RuntimeViewModel::busyChanged,
            this, &RuntimePanel::onBusyChanged);
    connect(&vm_, &ViewModels::RuntimeViewModel::statusMessageChanged,
            this, [this] { statusLabel_->setText(vm_.statusMessage()); });
}

QString RuntimePanel::selectedTargetName() const
{
    const auto rows = targetList_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* target = vm_.targetModel()->targetAt(rows.first().row());
    return target ? QString::fromStdString(target->name) : QString{};
}

void RuntimePanel::onTargetSelectionChanged()
{
    // Selecting a different target invalidates the previous packet
    // selection and any active telemetry poll - stop it rather than let it
    // keep polling a packet under the old target.
    vm_.setTelemetryPolling({}, {});
    reloadPacketsForSelection();
}

void RuntimePanel::onPacketKindChanged(int)
{
    vm_.setTelemetryPolling({}, {});
    reloadPacketsForSelection();
}

void RuntimePanel::reloadPacketsForSelection()
{
    const QString target = selectedTargetName();
    if (target.isEmpty()) return;

    if (packetKindCombo_->currentIndex() == 0)
        vm_.loadTelemetry(target);
    else
        vm_.loadCommands(target);
}

void RuntimePanel::onPacketSelectionChanged()
{
    // Only telemetry packets have live values to poll - selecting a command
    // packet leaves the values table showing whatever it last had, since a
    // command has no "current value" concept.
    if (packetKindCombo_->currentIndex() != 0) return;

    const auto rows = packetList_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        vm_.setTelemetryPolling({}, {});
        return;
    }

    const auto* packet = vm_.packetModel()->packetAt(rows.first().row());
    if (!packet) return;

    vm_.setTelemetryPolling(
        QString::fromStdString(packet->targetName),
        QString::fromStdString(packet->packetName));
}

void RuntimePanel::onBusyChanged()
{
    const bool busy = vm_.isBusy();
    refreshTargetsBtn_->setEnabled(!busy);
    packetKindCombo_->setEnabled(!busy);
}

} // namespace OpenC3::UI::Widgets
