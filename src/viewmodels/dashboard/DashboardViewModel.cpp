#include "DashboardViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <atomic>
#include <memory>

namespace OpenC3::ViewModels {

DashboardViewModel::DashboardViewModel(
    Services::IConnectionService& connection,
    Services::IDockerService&     docker,
    Services::ISystemService&     system,
    QObject*                      parent)
    : ViewModelBase(parent)
    , connection_(connection)
    , docker_(docker)
    , system_(system)
{
    timer_ = new QTimer(this);
    timer_->setInterval(5000); // refresh every 5 seconds
    connect(timer_, &QTimer::timeout, this, &DashboardViewModel::refresh);
    timer_->start();

    switch (connection_.state()) {
        case Services::ConnectionState::Connected:
            connectionStatus_ = "Connected";
            break;
        case Services::ConnectionState::Connecting:
            connectionStatus_ = "Connecting";
            break;
        case Services::ConnectionState::Disconnected:
            connectionStatus_ = "Disconnected";
            break;
        case Services::ConnectionState::Error:
            connectionStatus_ = "Error";
            break;
    }

    // Sync connection status — callback fires on the worker thread; marshal to GUI thread.
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
            connectionStatus_ = s;
            emit connectionStatusChanged();
        }, Qt::QueuedConnection);
    });
}

DashboardViewModel::~DashboardViewModel() = default;

// ── Property accessors ────────────────────────────────────────────────────────

QString DashboardViewModel::connectionStatus() const noexcept { return connectionStatus_; }
QString DashboardViewModel::dockerStatus()     const noexcept { return dockerStatus_;     }
QString DashboardViewModel::openC3Version()    const noexcept { return openC3Version_;    }
double  DashboardViewModel::cpuPercent()       const noexcept { return cpuPercent_;       }
double  DashboardViewModel::memPercent()       const noexcept { return memPercent_;       }
double  DashboardViewModel::diskPercent()      const noexcept { return diskPercent_;      }
int     DashboardViewModel::containerCount()   const noexcept { return containerCount_;   }

// ── Refresh ───────────────────────────────────────────────────────────────────

void DashboardViewModel::refresh()
{
    if (connection_.state() != Services::ConnectionState::Connected) return;

    setLoading(true);
    clearError();

    // Cleared to false once all three background fetches below have
    // completed (successfully or not) - shared_ptr so it stays alive
    // independent of this ViewModel even if a fetch is still in flight
    // when the ViewModel is destroyed.
    auto pending = std::make_shared<std::atomic<int>>(3);
    auto completeOne = [this, pending] {
        if (pending->fetch_sub(1, std::memory_order_acq_rel) == 1)
            setLoading(false);
    };

    // ── System metrics ────────────────────────────────────────────────────────
    auto* metricsWatcher = new QFutureWatcher<void>(this);
    connect(metricsWatcher, &QFutureWatcher<void>::finished,
            metricsWatcher, &QObject::deleteLater);

    auto metricsFuture = QtConcurrent::run([this, completeOne] {
        try {
            auto metrics = system_.getMetrics();
            double disk = metrics.disks.empty() ? 0.0
                        : metrics.disks[0].usedPercent;
            QMetaObject::invokeMethod(this, [this, completeOne,
                cpu  = metrics.cpuPercent,
                mem  = metrics.memPercent,
                d    = disk,
                host = QString::fromStdString(metrics.hostname)] {
                    onMetricsFetched(cpu, mem, d, host);
                    completeOne();
                }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            Logging::Logger::error("[DashboardVM] metrics error: {}", e.what());
            QMetaObject::invokeMethod(this, completeOne, Qt::QueuedConnection);
        }
    });
    metricsWatcher->setFuture(metricsFuture);

    // ── Docker status ─────────────────────────────────────────────────────────
    auto* dockerWatcher = new QFutureWatcher<void>(this);
    connect(dockerWatcher, &QFutureWatcher<void>::finished,
            dockerWatcher, &QObject::deleteLater);

    auto dockerFuture = QtConcurrent::run([this, completeOne] {
        try {
            bool daemonOk = docker_.isDockerRunning();
            auto containers = docker_.listContainers(false); // running only
            int  running    = static_cast<int>(containers.size());
            QMetaObject::invokeMethod(this, [this, completeOne, running, daemonOk] {
                onDockerFetched(running, daemonOk);
                completeOne();
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            Logging::Logger::error("[DashboardVM] docker error: {}", e.what());
            QMetaObject::invokeMethod(this, completeOne, Qt::QueuedConnection);
        }
    });
    dockerWatcher->setFuture(dockerFuture);

    // ── OpenC3 version ────────────────────────────────────────────────────────
    auto* versionWatcher = new QFutureWatcher<void>(this);
    connect(versionWatcher, &QFutureWatcher<void>::finished,
            versionWatcher, &QObject::deleteLater);

    auto versionFuture = QtConcurrent::run([this, completeOne] {
        try {
            auto v = system_.getOpenC3Version();
            QMetaObject::invokeMethod(this, [this, completeOne, ver = QString::fromStdString(v)] {
                onVersionFetched(ver);
                completeOne();
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            Logging::Logger::error("[DashboardVM] version error: {}", e.what());
            QMetaObject::invokeMethod(this, completeOne, Qt::QueuedConnection);
        }
    });
    versionWatcher->setFuture(versionFuture);
}

void DashboardViewModel::setRefreshIntervalMs(int ms)
{
    timer_->setInterval(ms);
}

// ── Private slots ─────────────────────────────────────────────────────────────

void DashboardViewModel::onMetricsFetched(
    double cpu, double mem, double disk, const QString& /*hostname*/)
{
    cpuPercent_  = cpu;
    memPercent_  = mem;
    diskPercent_ = disk;
    emit metricsChanged();
    emit refreshed();
}

void DashboardViewModel::onDockerFetched(int running, bool daemonOk)
{
    containerCount_ = running;
    dockerStatus_   = daemonOk
        ? QString("Running (%1 containers)").arg(running)
        : "Not running";
    emit dockerStatusChanged();
}

void DashboardViewModel::onVersionFetched(const QString& version)
{
    openC3Version_ = version;
    emit openC3VersionChanged();
}

} // namespace OpenC3::ViewModels
