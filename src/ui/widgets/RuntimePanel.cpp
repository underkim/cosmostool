#include "RuntimePanel.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace OpenC3::UI::Widgets {

namespace {

// Parses a simple "PARAM1=value1, PARAM2=value2" text into (name, value)
// pairs. Deliberately not COSMOS's own `cmd_string` grammar (quoting,
// arrays, etc.) - this is a minimal, honest parameter editor for the first
// write-path UI, not a full reimplementation of extract.rb's parser.
//
// Returns false (and leaves `params` untouched) when any comma-separated
// entry does not parse as NAME=VALUE, or has an empty NAME - a malformed
// entry must never be silently dropped on a command-sending path, since
// COSMOS would then send the command with that parameter left at its
// packet-defined default rather than what the operator actually typed.
bool parseParameterText(const QString& text, QVector<QPair<QString, QString>>& params,
                        QString& errorDetail)
{
    QVector<QPair<QString, QString>> parsed;
    const auto parts = text.split(',', Qt::SkipEmptyParts);
    for (const auto& part : parts) {
        const QString trimmedPart = part.trimmed();
        const auto eq = trimmedPart.indexOf('=');
        const QString name = eq >= 0 ? trimmedPart.left(eq).trimmed() : QString();
        if (eq < 0 || name.isEmpty()) {
            errorDetail = QObject::tr(
                "\"%1\" is not a valid PARAM=value entry.").arg(trimmedPart);
            return false;
        }
        const QString value = trimmedPart.mid(eq + 1).trimmed();
        parsed.append({name, value});
    }
    params = std::move(parsed);
    return true;
}

} // namespace

RuntimePanel::RuntimePanel(
    ViewModels::RuntimeViewModel& vm,
    ViewModels::CommandSenderViewModel& commandVm,
    QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
    , commandVm_(commandVm)
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

    sendCommandBtn_ = new QPushButton(tr("Send Command..."), this);
    sendCommandBtn_->setObjectName("DangerButton");
    sendCommandBtn_->setEnabled(false);
    sendCommandBtn_->setToolTip(tr(
        "Sends the selected command through the COSMOS API with full range "
        "and hazardous checks. A confirmation dialog always appears first; "
        "a hazardous command requires a second, explicit confirmation."));

    commandStatusLabel_ = new QLabel(tr("No command sent."), this);
    commandStatusLabel_->setObjectName("CommandSendStatus");
    commandStatusLabel_->setWordWrap(true);

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
    packetColumn->addWidget(sendCommandBtn_);
    packetColumn->addWidget(commandStatusLabel_);

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

    connect(sendCommandBtn_, &QPushButton::clicked,
            this, &RuntimePanel::onSendClicked);
    connect(&commandVm_, &ViewModels::CommandSenderViewModel::hazardousConfirmationNeeded,
            this, &RuntimePanel::onHazardousConfirmationNeeded);
    connect(&commandVm_, &ViewModels::CommandSenderViewModel::commandCompleted,
            this, &RuntimePanel::onCommandCompleted);
    connect(&commandVm_, &ViewModels::CommandSenderViewModel::busyChanged,
            this, &RuntimePanel::onBusyChanged);
}

QString RuntimePanel::selectedTargetName() const
{
    const auto rows = targetList_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* target = vm_.targetModel()->targetAt(rows.first().row());
    return target ? QString::fromStdString(target->name) : QString{};
}

bool RuntimePanel::isCommandKindSelected() const noexcept
{
    return packetKindCombo_->currentIndex() == 1;
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
    sendCommandBtn_->setEnabled(false);
}

void RuntimePanel::reloadPacketsForSelection()
{
    const QString target = selectedTargetName();
    if (target.isEmpty()) return;

    if (!isCommandKindSelected())
        vm_.loadTelemetry(target);
    else
        vm_.loadCommands(target);
}

void RuntimePanel::onPacketSelectionChanged()
{
    // Only telemetry packets have live values to poll - selecting a command
    // packet leaves the values table showing whatever it last had, since a
    // command has no "current value" concept.
    if (!isCommandKindSelected()) {
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
        return;
    }

    sendCommandBtn_->setEnabled(
        !commandVm_.isBusy() && packetList_->selectionModel()->hasSelection());
}

void RuntimePanel::onBusyChanged()
{
    const bool busy = vm_.isBusy() || commandVm_.isBusy();
    refreshTargetsBtn_->setEnabled(!vm_.isBusy());
    packetKindCombo_->setEnabled(!busy);
    if (isCommandKindSelected())
        sendCommandBtn_->setEnabled(
            !busy && packetList_->selectionModel()->hasSelection());
    else
        sendCommandBtn_->setEnabled(false);
}

void RuntimePanel::onSendClicked()
{
    if (!isCommandKindSelected()) return;
    const auto rows = packetList_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    const auto* packet = vm_.packetModel()->packetAt(rows.first().row());
    if (!packet) return;

    const QString target = QString::fromStdString(packet->targetName);
    const QString command = QString::fromStdString(packet->packetName);

    bool ok = false;
    const QString paramText = QInputDialog::getText(
        this, tr("Command Parameters"),
        tr("Enter parameters as PARAM1=value1, PARAM2=value2 (leave empty "
           "for a command with no parameters):"),
        QLineEdit::Normal, {}, &ok);
    if (!ok) return;

    QVector<QPair<QString, QString>> parameters;
    QString parseError;
    if (!parseParameterText(paramText, parameters, parseError)) {
        QMessageBox::warning(this, tr("Invalid Parameters"),
            tr("Could not send the command - %1\n\nUse PARAM1=value1, "
               "PARAM2=value2, one entry per parameter.").arg(parseError));
        return;
    }

    if (QMessageBox::question(this, tr("Confirm Send"),
            tr("Send this command to the connected COSMOS instance?\n\n%1 %2")
                .arg(target, command),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    commandVm_.send(target, command, parameters);
}

void RuntimePanel::onHazardousConfirmationNeeded(
    const QString& targetName, const QString& commandName, const QString& description)
{
    const QString detail = description.isEmpty()
        ? tr("COSMOS reports this command as hazardous.")
        : tr("COSMOS reports this command as hazardous:\n\n%1").arg(description);

    if (QMessageBox::warning(this, tr("Hazardous Command"),
            tr("%1\n\n%2 %3 will be sent WITHOUT the hazardous check if you "
               "confirm. Continue?").arg(detail, targetName, commandName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes)
    {
        commandVm_.sendConfirmed();
    } else {
        commandStatusLabel_->setText(
            tr("Hazardous command cancelled: %1 %2").arg(targetName, commandName));
    }
}

void RuntimePanel::onCommandCompleted(
    const QString& targetName, const QString& commandName, bool success, const QString& detail)
{
    commandStatusLabel_->setText(
        (success ? tr("Sent %1 %2\n%3") : tr("Failed %1 %2\n%3"))
            .arg(targetName, commandName, detail));
}

} // namespace OpenC3::UI::Widgets
