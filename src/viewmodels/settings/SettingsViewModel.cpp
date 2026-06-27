#include "SettingsViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>

namespace OpenC3::ViewModels {

// ── ProfileListModel ──────────────────────────────────────────────────────────

ProfileListModel::ProfileListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

void ProfileListModel::setProfiles(
    std::vector<Models::ConnectionProfile> profiles)
{
    beginResetModel();
    profiles_ = std::move(profiles);
    endResetModel();
}

int ProfileListModel::rowCount(const QModelIndex& p) const
{
    return p.isValid() ? 0 : static_cast<int>(profiles_.size());
}

QVariant ProfileListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& p = profiles_[static_cast<std::size_t>(index.row())];

    if (role == Qt::DisplayRole)
        return QString::fromStdString(p.name +
               (p.isDefault ? " (default)" : ""));

    if (role == Qt::UserRole)
        return QString::fromStdString(p.id);

    return {};
}

const Models::ConnectionProfile*
ProfileListModel::profileAt(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(profiles_.size())) return nullptr;
    return &profiles_[static_cast<std::size_t>(row)];
}

// ── SettingsViewModel ─────────────────────────────────────────────────────────

SettingsViewModel::SettingsViewModel(
    Services::ISettingsService&   settings,
    Services::IConnectionService& connection,
    QObject*                      parent)
    : ViewModelBase(parent)
    , settings_(settings)
    , connection_(connection)
    , profileModel_(new ProfileListModel(this))
{
    // Callback fires on the worker thread — marshal signal emission to the GUI thread.
    connection_.onStateChanged([this](const Services::ConnectionEvent& ev) {
        QString s;
        switch (ev.state) {
            case Services::ConnectionState::Connected:    s = "Connected";    break;
            case Services::ConnectionState::Connecting:   s = "Connecting";   break;
            case Services::ConnectionState::Disconnected: s = "Disconnected"; break;
            case Services::ConnectionState::Error:
                s = "Error: " + QString::fromStdString(ev.errorMessage); break;
        }
        QMetaObject::invokeMethod(this, [this, s] {
            emit connectionStateChanged(s);
        }, Qt::QueuedConnection);
    });
}

ProfileListModel* SettingsViewModel::profileModel() const noexcept
{
    return profileModel_;
}

bool SettingsViewModel::isConnected() const noexcept
{
    return connection_.state() == Services::ConnectionState::Connected;
}

void SettingsViewModel::loadProfiles()
{
    profileModel_->setProfiles(settings_.profiles());
    emit profilesChanged();
}

void SettingsViewModel::saveProfile(const Models::ConnectionProfile& profile)
{
    settings_.saveProfile(profile);
    loadProfiles();
}

void SettingsViewModel::deleteProfile(const QString& id)
{
    settings_.deleteProfile(id.toStdString());
    loadProfiles();
}

void SettingsViewModel::setDefaultProfile(const QString& id)
{
    settings_.setDefaultProfile(id.toStdString());
    loadProfiles();
}

void SettingsViewModel::connectToProfile(const QString& id)
{
    setLoading(true);
    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished,
            watcher, &QObject::deleteLater);

    auto future = QtConcurrent::run([this, sid = id.toStdString()] {
        (void)connection_.connect(sid);
        QMetaObject::invokeMethod(this, [this] {
            setLoading(false);
        }, Qt::QueuedConnection);
    });
    watcher->setFuture(future);
}

void SettingsViewModel::disconnect()
{
    connection_.disconnect();
}

} // namespace OpenC3::ViewModels
