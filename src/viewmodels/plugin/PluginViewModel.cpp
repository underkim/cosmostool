#include "PluginViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QColor>
#include <QStringList>

namespace OpenC3::ViewModels {

PluginTableModel::PluginTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void PluginTableModel::setPlugins(std::vector<Models::Plugin> plugins)
{
    beginResetModel();
    plugins_ = std::move(plugins);
    endResetModel();
}

const Models::Plugin* PluginTableModel::pluginAt(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(plugins_.size())) return nullptr;
    return &plugins_[static_cast<std::size_t>(row)];
}

int PluginTableModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(plugins_.size());
}

int PluginTableModel::columnCount(const QModelIndex&) const
{
    return ColCount;
}

QVariant PluginTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(plugins_.size()))
        return {};

    const auto& p = plugins_[static_cast<std::size_t>(idx.row())];

    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
        case Name:    return QString::fromStdString(p.name);
        case Version: return QString::fromStdString(p.version);
        case Author:  return QString::number(static_cast<int>(p.targets.size()));
        case Status:
            switch (p.status) {
            case Models::PluginStatus::Installed:        return "Ready";
            case Models::PluginStatus::Installing:       return "Building";
            case Models::PluginStatus::UpdateAvailable:  return "Update Available";
            case Models::PluginStatus::Error:            return "Needs Attention";
            default:                                     return "Unknown";
            }
        default: break;
        }
    }

    if (role == Qt::ForegroundRole && idx.column() == Status) {
        switch (p.status) {
        case Models::PluginStatus::Installed:       return QColor(0x4e, 0xc9, 0xb0);
        case Models::PluginStatus::UpdateAvailable: return QColor(0xce, 0x91, 0x78);
        case Models::PluginStatus::Error:           return QColor(0xf4, 0x47, 0x47);
        default:                                    return QColor(0x85, 0x85, 0x85);
        }
    }

    if (role == Qt::ToolTipRole)
        return QString::fromStdString(p.rootPath);

    return {};
}

QVariant PluginTableModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case Name:    return "Plugin Folder";
    case Version: return "Version";
    case Status:  return "Status";
    case Author:  return "Targets";
    default:      return {};
    }
}

PluginViewModel::PluginViewModel(
    Services::IPluginService&     plugin,
    Services::IConnectionService& connection,
    QObject*                      parent)
    : ViewModelBase(parent)
    , plugin_(plugin)
    , connection_(connection)
    , tableModel_(new PluginTableModel(this))
{}

PluginTableModel* PluginViewModel::pluginModel() const noexcept
{
    return tableModel_;
}

bool PluginViewModel::isBusy() const noexcept { return busy_; }

QString PluginViewModel::statusMessage() const noexcept { return statusMessage_; }

QString PluginViewModel::pluginRootPath() const noexcept
{
    QString path = QString::fromStdString(connection_.cosmosRootPath()).trimmed();
    if (path.isEmpty()) return "/cosmos";

    while (path.size() > 1 && path.endsWith('/'))
        path.chop(1);

    if (path.endsWith("/openc3.sh") || path == "openc3.sh") {
        const int slash = path.lastIndexOf('/');
        return slash > 0 ? path.left(slash) : ".";
    }

    return path;
}

void PluginViewModel::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void PluginViewModel::setStatus(const QString& msg)
{
    statusMessage_ = msg;
    emit statusMessageChanged();
}

void PluginViewModel::refresh()
{
    if (busy_) return;
    setBusy(true);
    setStatus("Loading plugin folders");

    const std::string cosmosRoot = pluginRootPath().toStdString();
    (void)QtConcurrent::run([this, cosmosRoot] {
        auto plugins = plugin_.listInstalled(cosmosRoot);
        QMetaObject::invokeMethod(this, [this, list = std::move(plugins)] () mutable {
            tableModel_->setPlugins(std::move(list));
            emit pluginListChanged();
            setBusy(false);
            setStatus(QString("Loaded %1 plugin folder(s)").arg(tableModel_->rowCount()));
        }, Qt::QueuedConnection);
    });
}

void PluginViewModel::install(const QString& gemFilePath)
{
    if (busy_) return;
    const std::string path       = gemFilePath.toStdString();
    const std::string cosmosRoot = pluginRootPath().toStdString();
    setBusy(true);
    setStatus("Installing " + gemFilePath);

    (void)QtConcurrent::run([this, path, cosmosRoot] {
        const bool ok = plugin_.install(path, cosmosRoot);
        QMetaObject::invokeMethod(this, [this, ok, qpath = QString::fromStdString(path)] {
            emit actionCompleted(qpath, ok);
            setBusy(false);
            if (ok) {
                setStatus("Installed successfully.");
                refresh();
            } else {
                setStatus("Install failed.");
            }
        }, Qt::QueuedConnection);
    });
}

void PluginViewModel::remove(const QString& pluginName)
{
    if (busy_) return;
    const std::string name       = pluginName.toStdString();
    const std::string cosmosRoot = pluginRootPath().toStdString();
    setBusy(true);
    setStatus("Removing " + pluginName);

    (void)QtConcurrent::run([this, name, cosmosRoot] {
        const bool ok = plugin_.remove(name, cosmosRoot);
        QMetaObject::invokeMethod(this, [this, ok, qname = QString::fromStdString(name)] {
            emit actionCompleted(qname, ok);
            setBusy(false);
            if (ok) {
                setStatus("Removed successfully.");
                refresh();
            } else {
                setStatus("Remove failed.");
            }
        }, Qt::QueuedConnection);
    });
}

void PluginViewModel::validate(const QString& localPath)
{
    if (busy_) return;
    const std::string path = localPath.toStdString();
    setBusy(true);
    setStatus("Validating " + localPath);

    (void)QtConcurrent::run([this, path] {
        const auto result = plugin_.validate(path);
        QMetaObject::invokeMethod(this, [this, result] {
            setBusy(false);
            QString summary;
            if (result.valid) {
                summary = "Plugin folder is valid.";
            } else {
                QStringList msgs;
                for (const auto& issue : result.issues)
                    msgs << QString("[%1] %2").arg(
                        QString::fromStdString(issue.code),
                        QString::fromStdString(issue.message));
                summary = msgs.join('\n');
            }
            setStatus(result.valid ? "Valid" : "Validation failed");
            emit validationComplete(result.valid, summary);
        }, Qt::QueuedConnection);
    });
}

void PluginViewModel::build(const QString& pluginRootPath)
{
    if (busy_) return;
    const std::string path = pluginRootPath.toStdString();
    setBusy(true);
    setStatus("Building " + pluginRootPath);

    (void)QtConcurrent::run([this, path] {
        const auto output = plugin_.build(path);
        QMetaObject::invokeMethod(this, [this,
            qpath = QString::fromStdString(path),
            qout  = QString::fromStdString(output)] {
                setBusy(false);
                const bool ok = !qout.trimmed().isEmpty()
                    && !qout.contains("ERROR", Qt::CaseInsensitive)
                    && !qout.contains("failed", Qt::CaseInsensitive);
                setStatus(ok ? "Build complete" : "Build failed");
                emit validationComplete(ok, qout);
                emit actionCompleted(qpath, ok);
                if (ok) refresh();
            }, Qt::QueuedConnection);
    });
}

} // namespace OpenC3::ViewModels
