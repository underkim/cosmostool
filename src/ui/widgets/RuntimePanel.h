#pragma once

#include "viewmodels/runtime/RuntimeViewModel.h"
#include "viewmodels/runtime/CommandSenderViewModel.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListView;
class QPushButton;
class QTableView;

namespace OpenC3::UI::Widgets {

/// "COSMOS Targets" panel: browses targets and command/telemetry packets
/// actually known to a running COSMOS (through ICosmosRuntimeService's
/// JSON-DRB calls), polls current telemetry values for a selected telemetry
/// packet, and - only for a selected *command* packet - can send it through
/// ICosmosCommandService via CommandSenderViewModel.
///
/// Commands and telemetry packets share one list (a Commands/Telemetry
/// toggle switches which is loaded) because RuntimeViewModel's
/// PacketListModel is deliberately one model for both - COSMOS itself
/// answers "what commands" and "what telemetry" as two separate calls under
/// one target, not two permanently-coexisting lists.
///
/// Sending is deliberately conservative: a confirmation dialog before every
/// send (not just hazardous ones), and a second, more strongly-worded
/// dialog when COSMOS reports the command as hazardous
/// (hazardousConfirmationNeeded) - the command is only ever resent with the
/// hazardous check bypassed after that second, explicit confirmation.
class RuntimePanel final : public QWidget {
    Q_OBJECT
public:
    explicit RuntimePanel(
        ViewModels::RuntimeViewModel&        vm,
        ViewModels::CommandSenderViewModel&  commandVm,
        QWidget*                             parent = nullptr);

private slots:
    void onTargetSelectionChanged();
    void onPacketKindChanged(int index);
    void onPacketSelectionChanged();
    void onBusyChanged();
    void onSendClicked();
    void onHazardousConfirmationNeeded(const QString& targetName,
                                       const QString& commandName,
                                       const QString& description);
    void onCommandCompleted(const QString& targetName, const QString& commandName,
                            bool success, const QString& detail);

private:
    ViewModels::RuntimeViewModel&       vm_;
    ViewModels::CommandSenderViewModel& commandVm_;

    QPushButton* refreshTargetsBtn_{nullptr};
    QListView*   targetList_{nullptr};
    QComboBox*   packetKindCombo_{nullptr};
    QListView*   packetList_{nullptr};
    QTableView*  telemetryTable_{nullptr};
    QPushButton* sendCommandBtn_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QLabel*      commandStatusLabel_{nullptr};

    [[nodiscard]] QString selectedTargetName() const;
    void reloadPacketsForSelection();
    [[nodiscard]] bool isCommandKindSelected() const noexcept;
};

} // namespace OpenC3::UI::Widgets
