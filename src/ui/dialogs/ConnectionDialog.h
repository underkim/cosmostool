#pragma once

#include "viewmodels/settings/SettingsViewModel.h"
#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QString>
#include <QStringList>

namespace OpenC3::UI::Dialogs {

/// Detects installed WSL distros (via `wsl.exe --list --quiet`), for the
/// Quick-WSL one-click profile flow - shared with MainWindow's silent
/// auto-connect (Plugin Creation mode), which needs the same detection
/// without a visible dialog.
[[nodiscard]] QStringList detectWslDistros();

/// Quick-connect dialog shown on startup if no default profile is configured.
class ConnectionDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(
        ViewModels::SettingsViewModel& vm,
        QWidget* parent = nullptr);

private slots:
    void onConnectClicked();
    void onCreateWslProfile();

private:
    ViewModels::SettingsViewModel& vm_;
    QComboBox*   profileCombo_{nullptr};
    QPushButton* connectBtn_{nullptr};
    QPushButton* quickWslBtn_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QString      quickWslDistro_;
};

} // namespace OpenC3::UI::Dialogs
