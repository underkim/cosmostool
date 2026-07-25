#pragma once

#include "viewmodels/plugin/PluginViewModel.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTableView;

namespace OpenC3::UI::Widgets {

/// "Installed in COSMOS" panel: shows PluginViewModel's InstalledPluginTableModel
/// (populated through ICosmosPluginService, never Workspace source folders),
/// lets the user refresh it and uninstall by the exact suffixed instance id
/// (`gem__0`), and surfaces the ProcessManager job-status text that tracks an
/// install/uninstall started elsewhere in the wizard through to a terminal
/// state.
///
/// Split out of PluginView (which had grown past 3600 lines) as a
/// self-contained unit: it owns its own widgets and wiring against
/// PluginViewModel and does not reach into PluginView's Workspace-source
/// state (currentPluginRoot_, tableView_, etc.) at all - the two tables are
/// deliberately unrelated (see ICosmosPluginService's header comment on why
/// installed instances and source folders are not the same thing).
class InstalledPluginsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit InstalledPluginsPanel(
        ViewModels::PluginViewModel& vm,
        QWidget*                     parent = nullptr);

private slots:
    void onRemoveClicked();
    void onSelectionOrBusyChanged();

private:
    ViewModels::PluginViewModel& vm_;

    QPushButton* refreshBtn_{nullptr};
    QPushButton* removeBtn_{nullptr};
    QLabel*      jobStatusLabel_{nullptr};
    QTableView*  tableView_{nullptr};
};

} // namespace OpenC3::UI::Widgets
