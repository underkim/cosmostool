#include "DockerViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QColor>

namespace OpenC3::ViewModels {

// ── ContainerTableModel ───────────────────────────────────────────────────────

ContainerTableModel::ContainerTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void ContainerTableModel::setContainers(
    std::vector<Models::DockerContainer> containers)
{
    beginResetModel();
    containers_ = std::move(containers);
    endResetModel();
}

int ContainerTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(containers_.size());
}

int ContainerTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return Column::ColCount;
}

QVariant ContainerTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& c = containers_[static_cast<std::size_t>(index.row())];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Column::Name:   return QString::fromStdString(c.name);
            case Column::Image:  return QString::fromStdString(c.image);
            case Column::Status: return QString::fromStdString(c.statusText);
            case Column::Ports: {
                QString ports;
                for (const auto& p : c.ports)
                    ports += QString("%1->%2/%3 ")
                        .arg(p.publicPort).arg(p.privatePort)
                        .arg(QString::fromStdString(p.type));
                return ports.trimmed();
            }
            default: return {};
        }
    }

    if (role == Qt::ForegroundRole && index.column() == Column::Status) {
        switch (c.status) {
            case Models::ContainerStatus::Running:
                return QColor(0x4e, 0xc9, 0x4e);   // green
            case Models::ContainerStatus::Exited:
            case Models::ContainerStatus::Dead:
                return QColor(0xf1, 0x44, 0x4c);   // red
            case Models::ContainerStatus::Paused:
            case Models::ContainerStatus::Restarting:
                return QColor(0xcd, 0x91, 0x00);   // amber
            default:
                return QColor(0x9d, 0x9d, 0x9d);   // grey
        }
    }

    return {};
}

QVariant ContainerTableModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case Column::Name:   return "Name";
        case Column::Image:  return "Image";
        case Column::Status: return "Status";
        case Column::Ports:  return "Ports";
        default:             return {};
    }
}

const Models::DockerContainer*
ContainerTableModel::containerAt(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(containers_.size())) return nullptr;
    return &containers_[static_cast<std::size_t>(row)];
}

// ── DockerViewModel ───────────────────────────────────────────────────────────

DockerViewModel::DockerViewModel(
    Services::IConnectionService& connection,
    Services::IDockerService&     docker,
    QObject*                      parent)
    : ViewModelBase(parent)
    , connection_(connection)
    , docker_(docker)
    , containerModel_(new ContainerTableModel(this))
{
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(8000);
    connect(refreshTimer_, &QTimer::timeout, this, &DockerViewModel::refresh);
    refreshTimer_->start();

    // Let the view react to connect/disconnect (hint text, button state) and
    // populate the list immediately after a successful connection.
    connection_.onStateChanged([this](const Services::ConnectionEvent& ev) {
        Q_UNUSED(ev);
        QMetaObject::invokeMethod(this, [this] {
            emit connectionChanged();
            if (isConnected()) refresh();
        }, Qt::QueuedConnection);
    });
}

bool DockerViewModel::isConnected() const noexcept
{
    return connection_.state() == Services::ConnectionState::Connected;
}

ContainerTableModel* DockerViewModel::containerModel() const noexcept
{
    return containerModel_;
}

QString DockerViewModel::dockerVersion() const noexcept
{
    return dockerVersion_;
}

void DockerViewModel::refresh()
{
    if (connection_.state() != Services::ConnectionState::Connected) return;

    setLoading(true);

    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished,
            watcher, &QObject::deleteLater);

    auto future = QtConcurrent::run([this] {
        try {
            auto containers = docker_.listContainers(true);
            auto version    = docker_.dockerVersion();
            QMetaObject::invokeMethod(this, [this,
                c = std::move(containers),
                v = QString::fromStdString(version)] () mutable {
                    containerModel_->setContainers(std::move(c));
                    dockerVersion_ = v;
                    emit dockerVersionChanged();
                    setLoading(false);
                    emit refreshed();
                }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            Logging::Logger::error("[DockerVM] refresh error: {}", e.what());
            QMetaObject::invokeMethod(this, [this, msg = std::string(e.what())] {
                setError(QString::fromStdString(msg));
                setLoading(false);
            }, Qt::QueuedConnection);
        }
    });
    watcher->setFuture(future);
}

void DockerViewModel::startContainer(const QString& nameOrId)
{
    (void)QtConcurrent::run([this, id = nameOrId.toStdString()] {
        bool ok = docker_.startContainer(id);
        QMetaObject::invokeMethod(this, [this, ok, qid = QString::fromStdString(id)] {
            emit containerActionCompleted(qid, ok);
            refresh();
        }, Qt::QueuedConnection);
    });
}

void DockerViewModel::stopContainer(const QString& nameOrId)
{
    (void)QtConcurrent::run([this, id = nameOrId.toStdString()] {
        bool ok = docker_.stopContainer(id);
        QMetaObject::invokeMethod(this, [this, ok, qid = QString::fromStdString(id)] {
            emit containerActionCompleted(qid, ok);
            refresh();
        }, Qt::QueuedConnection);
    });
}

void DockerViewModel::restartContainer(const QString& nameOrId)
{
    (void)QtConcurrent::run([this, id = nameOrId.toStdString()] {
        bool ok = docker_.restartContainer(id);
        QMetaObject::invokeMethod(this, [this, ok, qid = QString::fromStdString(id)] {
            emit containerActionCompleted(qid, ok);
            refresh();
        }, Qt::QueuedConnection);
    });
}

void DockerViewModel::removeContainer(const QString& nameOrId)
{
    (void)QtConcurrent::run([this, id = nameOrId.toStdString()] {
        bool ok = docker_.removeContainer(id, false);
        QMetaObject::invokeMethod(this, [this, ok, qid = QString::fromStdString(id)] {
            emit containerActionCompleted(qid, ok);
            refresh();
        }, Qt::QueuedConnection);
    });
}

void DockerViewModel::fetchLogs(const QString& nameOrId, int tail)
{
    (void)QtConcurrent::run([this, id = nameOrId.toStdString(), tail] {
        auto logs = docker_.getLogs(id, tail);
        QMetaObject::invokeMethod(this, [this,
            qid   = QString::fromStdString(id),
            qlogs = QString::fromStdString(logs)] {
                emit logsReady(qid, qlogs);
            }, Qt::QueuedConnection);
    });
}

} // namespace OpenC3::ViewModels
