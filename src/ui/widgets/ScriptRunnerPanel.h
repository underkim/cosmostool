#pragma once

#include "viewmodels/script/ScriptViewModel.h"

#include <QWidget>

class QLabel;
class QListView;
class QPushButton;

namespace OpenC3::UI::Widgets {

/// "COSMOS Script Runner" panel: lists scripts actually deployed in the
/// connected scope's bucket storage (through IScriptService, never Workspace
/// source folders or PluginView's separate host-shell "Run Script"
/// button), starts one disconnected, and tracks it through to a terminal
/// ScriptStatusModel state.
///
/// Deliberately does not try to auto-select "whatever file is open in the
/// editor": PluginView's editor works on local WSL/SSH source files, while
/// scripts here are bucket-relative paths that only exist once the owning
/// plugin has actually been installed into COSMOS (see
/// ICosmosPluginService) - the two lists share no reliable 1:1 mapping, so
/// making the user pick from the real list here is honest rather than
/// silently guessing.
class ScriptRunnerPanel final : public QWidget {
    Q_OBJECT
public:
    explicit ScriptRunnerPanel(
        ViewModels::ScriptViewModel& vm,
        QWidget*                     parent = nullptr);

private slots:
    void onRunClicked();
    void onSelectionOrBusyChanged();

private:
    ViewModels::ScriptViewModel& vm_;

    QPushButton* refreshBtn_{nullptr};
    QPushButton* runBtn_{nullptr};
    QPushButton* stopBtn_{nullptr};
    QPushButton* pauseBtn_{nullptr};
    QPushButton* resumeBtn_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QListView*   listView_{nullptr};
};

} // namespace OpenC3::UI::Widgets
