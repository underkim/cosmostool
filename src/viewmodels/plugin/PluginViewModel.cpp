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
        case TargetCount: return QString::number(static_cast<int>(p.targets.size()));
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
    case TargetCount: return "Targets";
    default:      return {};
    }
}

PluginViewModel::PluginViewModel(
    Services::IWorkspaceService&    workspace,
    Services::ICosmosPluginService& cosmosPlugin,
    Services::IConnectionService&   connection,
    QObject*                        parent)
    : ViewModelBase(parent)
    , workspace_(workspace)
    , cosmosPlugin_(cosmosPlugin)
    , connection_(connection)
    , tableModel_(new PluginTableModel(this))
{}

PluginViewModel::~PluginViewModel()
{
    // Blocks until any in-flight worker finishes. The worker captures `this`
    // and posts a queued invocation back to this object, so returning here
    // while it runs would leave it writing into freed memory.
    //
    // Safe against the install/remove completion handlers calling refresh():
    // that happens on the GUI thread inside the queued invocation, by which
    // point the original worker has already returned, so worker_ is only ever
    // reassigned when the future it replaces has finished.
    if (worker_.isRunning())
        worker_.waitForFinished();
}

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
        const qsizetype slash = path.lastIndexOf('/');
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
    worker_ = QtConcurrent::run([this, cosmosRoot] {
        auto plugins = workspace_.listSources(cosmosRoot);
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
    const std::string path = gemFilePath.toStdString();

    // Installation goes through the COSMOS API, not the shell. The previous
    // implementation copied the .gem into <cosmosRoot>/plugins/ and reported
    // success, but that has no effect on a running COSMOS - installed plugins
    // live in MinIO + Redis, and installing is a two-phase server-side job.
    if (!cosmosPlugin_.isAvailable()) {
        setStatus("Not connected to the COSMOS API - "
                  "set an API password in the connection profile to install plugins.");
        emit actionCompleted(gemFilePath, false);
        return;
    }

    setBusy(true);
    setStatus("Installing " + gemFilePath);

    worker_ = QtConcurrent::run([this, path] {
        const auto result = cosmosPlugin_.installGem(path);
        QMetaObject::invokeMethod(this,
            [this, result, qpath = QString::fromStdString(path)] {
                emit actionCompleted(qpath, result.success);
                setBusy(false);
                // COSMOS spawns openc3cli and returns immediately, so this
                // reports that the job started - result.message says as much
                // rather than claiming a finished install.
                const QString outcome = QString::fromStdString(
                    result.success ? result.message
                                   : "Install failed: " + result.message);
                if (result.success) {
                    // refresh() overwrites statusMessage with its own
                    // "Loading..." text, which would erase the outcome the
                    // user needs to read. Re-assert it afterwards so the
                    // result survives the reload.
                    refresh();
                }
                setStatus(outcome);
            }, Qt::QueuedConnection);
    });
}

void PluginViewModel::remove(const QString& pluginName)
{
    if (busy_) return;
    const std::string name = pluginName.toStdString();

    // COSMOS 6.10.4 PluginModel#create assigns every installed instance an
    // `__<counter>` suffix, including the first one (__0). Workspace rows are
    // source-folder names and do not carry that identity. Sending a source
    // name to DELETE /plugins/:id either fails or can target the wrong thing;
    // multiple instances make guessing fundamentally ambiguous. Refuse the
    // unsafe operation until the UI exposes an Installed Plugins list whose
    // rows come from ICosmosPluginService::listInstalled().
    if (!pluginName.contains("__")) {
        setStatus("Remove requires an installed COSMOS plugin instance "
                  "(for example name.gem__0). The selected Workspace row is "
                  "a source folder, not an installed instance.");
        emit actionCompleted(pluginName, false);
        return;
    }

    if (!cosmosPlugin_.isAvailable()) {
        setStatus("Not connected to the COSMOS API - "
                  "set an API password in the connection profile to remove plugins.");
        emit actionCompleted(pluginName, false);
        return;
    }

    setBusy(true);
    setStatus("Removing " + pluginName);

    worker_ = QtConcurrent::run([this, name] {
        const auto result = cosmosPlugin_.uninstall(name);
        QMetaObject::invokeMethod(this,
            [this, result, qname = QString::fromStdString(name)] {
                emit actionCompleted(qname, result.success);
                setBusy(false);
                // Same ordering hazard as install(): refresh() would clobber
                // the outcome message, so re-assert it after the reload.
                const QString outcome = QString::fromStdString(
                    result.success ? result.message
                                   : "Remove failed: " + result.message);
                if (result.success) refresh();
                setStatus(outcome);
            }, Qt::QueuedConnection);
    });
}

void PluginViewModel::validate(const QString& localPath)
{
    if (busy_) return;
    const std::string path = localPath.toStdString();
    setBusy(true);
    setStatus("Validating " + localPath);

    worker_ = QtConcurrent::run([this, path] {
        const auto result = workspace_.validate(path);
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

    worker_ = QtConcurrent::run([this, path] {
        const auto output = workspace_.build(path);
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
