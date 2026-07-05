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

    [[nodiscard]] ProfileListModel* profileModel() const noexcept;
    [[nodiscard]] bool isConnected() const noexcept;

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
};

} // namespace OpenC3::ViewModels
