#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/connection/IConnectionService.h"
#include "services/docker/IDockerService.h"
#include "services/system/ISystemService.h"

#include <QTimer>
#include <QString>
#include <memory>

namespace OpenC3::ViewModels {

/// ViewModel for the Dashboard module.
///
/// Periodically polls system metrics, Docker status and OpenC3 version
/// via background futures and emits Qt signals when data is ready.
class DashboardViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
    Q_PROPERTY(QString dockerStatus     READ dockerStatus     NOTIFY dockerStatusChanged)
    Q_PROPERTY(QString openC3Version    READ openC3Version    NOTIFY openC3VersionChanged)
    Q_PROPERTY(double  cpuPercent       READ cpuPercent       NOTIFY metricsChanged)
    Q_PROPERTY(double  memPercent       READ memPercent       NOTIFY metricsChanged)
    Q_PROPERTY(double  diskPercent      READ diskPercent      NOTIFY metricsChanged)
    Q_PROPERTY(int     containerCount   READ containerCount   NOTIFY dockerStatusChanged)

public:
    explicit DashboardViewModel(
        Services::IConnectionService& connection,
        Services::IDockerService&     docker,
        Services::ISystemService&     system,
        QObject*                      parent = nullptr);

    ~DashboardViewModel() override;

    [[nodiscard]] QString connectionStatus() const noexcept;
    [[nodiscard]] QString dockerStatus()     const noexcept;
    [[nodiscard]] QString openC3Version()    const noexcept;
    [[nodiscard]] double  cpuPercent()       const noexcept;
    [[nodiscard]] double  memPercent()       const noexcept;
    [[nodiscard]] double  diskPercent()      const noexcept;
    [[nodiscard]] int     containerCount()   const noexcept;

    // Raw state for view-layer logic (badge style, guidance branching) that
    // must not depend on the (now-translated) display text returned above.
    [[nodiscard]] Services::ConnectionState connectionState() const noexcept;
    [[nodiscard]] bool                      isDockerRunning() const noexcept;

public slots:
    void refresh();
    void setRefreshIntervalMs(int ms);

signals:
    void connectionStatusChanged();
    void dockerStatusChanged();
    void openC3VersionChanged();
    void metricsChanged();

private slots:
    void onMetricsFetched(double cpu, double mem, double disk,
                          const QString& hostname);
    void onDockerFetched(int running, bool daemonOk);
    void onVersionFetched(const QString& version);

private:
    Services::IConnectionService& connection_;
    Services::IDockerService&     docker_;
    Services::ISystemService&     system_;

    QTimer* timer_{nullptr};
    Services::IConnectionService::SubscriptionId connSubscription_{0};

    Services::ConnectionState connectionState_{Services::ConnectionState::Disconnected};
    QString connectionErrorMessage_;
    bool    dockerRunning_{false};
    QString openC3Version_{"Unknown"};
    double  cpuPercent_{0.0};
    double  memPercent_{0.0};
    double  diskPercent_{0.0};
    int     containerCount_{0};
};

} // namespace OpenC3::ViewModels
