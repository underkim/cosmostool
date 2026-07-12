#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/settings/ISettingsService.h"
#include "services/connection/IConnectionService.h"
#include "models/ConnectionProfile.h"

#include <QAbstractListModel>
#include <vector>

namespace OpenC3::ViewModels {

class ProfileListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ProfileListModel(QObject* parent = nullptr);
    void setProfiles(std::vector<Models::ConnectionProfile> profiles);

    [[nodiscard]] int      rowCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int) const override;

    [[nodiscard]] const Models::ConnectionProfile* profileAt(int row) const noexcept;

private:
    std::vector<Models::ConnectionProfile> profiles_;
};

// ─────────────────────────────────────────────────────────────────────────────

class SettingsViewModel final : public ViewModelBase {
    Q_OBJECT

public:
    explicit SettingsViewModel(
        Services::ISettingsService&  settings,
        Services::IConnectionService& connection,
        QObject*                     parent = nullptr);

    ~SettingsViewModel() override;

    [[nodiscard]] ProfileListModel* profileModel() const noexcept;
    [[nodiscard]] bool isConnected() const noexcept;

    // Empty if no profile is marked default. Exposed for MainWindow's silent
    // auto-connect (Plugin Creation mode) - reuses the service-layer concept
    // that already existed (ISettingsService::defaultProfile()) rather than
    // making callers scan profileModel() rows for isDefault themselves.
    [[nodiscard]] QString defaultProfileId() const;

    // Live query, not the last-received connectionStateChanged event - lets a
    // view constructed *after* a connection already succeeded elsewhere (e.g.
    // the startup ConnectionDialog, which runs and can connect before
    // MainWindow/SettingsView exist) sync itself to the real current state
    // instead of assuming Disconnected until the next transition.
    [[nodiscard]] Services::ConnectionState connectionState() const noexcept;

public slots:
    void loadProfiles();
    void saveProfile(const Models::ConnectionProfile& profile);
    void deleteProfile(const QString& id);
    void setDefaultProfile(const QString& id);
    void connectToProfile(const QString& id);
    void disconnect();

signals:
    void profilesChanged();
    void connectionStateChanged(Services::ConnectionState state, const QString& errorMessage);

private:
    Services::ISettingsService&  settings_;
    Services::IConnectionService& connection_;
    ProfileListModel*             profileModel_{nullptr};
    Services::IConnectionService::SubscriptionId connSubscription_{0};
};

} // namespace OpenC3::ViewModels
