#pragma once

#include "viewmodels/settings/SettingsViewModel.h"

#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QStackedWidget>
#include <QString>

namespace OpenC3::UI::Views {

class SettingsView final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsView(
        ViewModels::SettingsViewModel& vm,
        QWidget* parent = nullptr);

private slots:
    void onAddProfile();
    void onQuickWslProfile();
    void onDeleteProfile();
    void onConnectClicked();
    void onDisconnectClicked();
    void onProfileSelected(const QModelIndex& idx);
    void onModeChanged(int modeIndex);
    void refreshWslDistros();
    void detectOpenC3Path();

private:
    void setupUi();
    void bindViewModel();
    void populateProfileForm(const Models::ConnectionProfile& p);
    Models::ConnectionProfile collectProfileForm() const;
    void updateConnectionButtons(const QString& state);
    void updateProfileSelectionUi();
    void updateActionHints(const QString& state);

    ViewModels::SettingsViewModel& vm_;
    QString connectionState_{"Disconnected"};

    // Left pane — profile list
    QListView*   profileList_{nullptr};
    QPushButton* addBtn_{nullptr};
    QPushButton* quickWslBtn_{nullptr};
    QPushButton* deleteBtn_{nullptr};
    QPushButton* connectBtn_{nullptr};
    QPushButton* disconnectBtn_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QLabel*      profileHintLabel_{nullptr};

    // Right pane — common fields
    QLineEdit*      nameEdit_{nullptr};
    QComboBox*      modeCombo_{nullptr};

    // Mode-specific panel (stacked)
    QStackedWidget* modeStack_{nullptr};

    // WSL page (index 0)
    QComboBox*   wslDistroCombo_{nullptr};
    QPushButton* wslRefreshBtn_{nullptr};

    // Common field (shown for both WSL and SSH)
    QLineEdit*   cosmosRootPathEdit_{nullptr};
    QPushButton* detectOpenC3PathBtn_{nullptr};

    // SSH page (index 1)
    QLineEdit*   hostEdit_{nullptr};
    QLineEdit*   portEdit_{nullptr};
    QLineEdit*   usernameEdit_{nullptr};
    QComboBox*   authMethodCombo_{nullptr};
    QLineEdit*   passwordEdit_{nullptr};
    QLineEdit*   keyPathEdit_{nullptr};

    QLabel*      profileFormStateLabel_{nullptr};
    QPushButton* saveProfileBtn_{nullptr};
    QPushButton* saveAndConnectBtn_{nullptr};
};

} // namespace OpenC3::UI::Views
