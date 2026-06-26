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

namespace OpenC3::UI::Views {

class SettingsView final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsView(
        ViewModels::SettingsViewModel& vm,
        QWidget* parent = nullptr);

private slots:
    void onAddProfile();
    void onDeleteProfile();
    void onConnectClicked();
    void onDisconnectClicked();
    void onProfileSelected(const QModelIndex& idx);

private:
    void setupUi();
    void bindViewModel();
    void populateProfileForm(const Models::ConnectionProfile& p);
    Models::ConnectionProfile collectProfileForm() const;

    ViewModels::SettingsViewModel& vm_;

    QListView*   profileList_{nullptr};
    QPushButton* addBtn_{nullptr};
    QPushButton* deleteBtn_{nullptr};
    QPushButton* connectBtn_{nullptr};
    QPushButton* disconnectBtn_{nullptr};

    // Profile form
    QLineEdit*   nameEdit_{nullptr};
    QComboBox*   modeCombo_{nullptr};
    QLineEdit*   wslDistroEdit_{nullptr};
    QLineEdit*   hostEdit_{nullptr};
    QLineEdit*   portEdit_{nullptr};
    QLineEdit*   usernameEdit_{nullptr};
    QComboBox*   authMethodCombo_{nullptr};
    QLineEdit*   keyPathEdit_{nullptr};
    QPushButton* saveProfileBtn_{nullptr};

    QLabel* statusLabel_{nullptr};
};

} // namespace OpenC3::UI::Views
