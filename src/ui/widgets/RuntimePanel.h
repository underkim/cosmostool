#pragma once

#include "viewmodels/runtime/RuntimeViewModel.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListView;
class QPushButton;
class QTableView;

namespace OpenC3::UI::Widgets {

/// Read-only "COSMOS Targets" panel: browses targets and command/telemetry
/// packets actually known to a running COSMOS (through
/// ICosmosRuntimeService's JSON-DRB calls), and polls current telemetry
/// values for a selected telemetry packet. Never sends a command - see
/// ICosmosRuntimeService's header for why write access is out of scope for
/// now.
///
/// Commands and telemetry packets share one list (a Commands/Telemetry
/// toggle switches which is loaded) because RuntimeViewModel's
/// PacketListModel is deliberately one model for both - COSMOS itself
/// answers "what commands" and "what telemetry" as two separate calls under
/// one target, not two permanently-coexisting lists.
class RuntimePanel final : public QWidget {
    Q_OBJECT
public:
    explicit RuntimePanel(
        ViewModels::RuntimeViewModel& vm,
        QWidget*                      parent = nullptr);

private slots:
    void onTargetSelectionChanged();
    void onPacketKindChanged(int index);
    void onPacketSelectionChanged();
    void onBusyChanged();

private:
    ViewModels::RuntimeViewModel& vm_;

    QPushButton* refreshTargetsBtn_{nullptr};
    QListView*   targetList_{nullptr};
    QComboBox*   packetKindCombo_{nullptr};
    QListView*   packetList_{nullptr};
    QTableView*  telemetryTable_{nullptr};
    QLabel*      statusLabel_{nullptr};

    [[nodiscard]] QString selectedTargetName() const;
    void reloadPacketsForSelection();
};

} // namespace OpenC3::UI::Widgets
