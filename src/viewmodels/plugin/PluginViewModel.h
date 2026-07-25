#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/workspace/IWorkspaceService.h"
#include "services/plugin/ICosmosPluginService.h"
#include "services/connection/IConnectionService.h"
#include "models/Plugin.h"

#include <QFuture>

#include <QAbstractTableModel>
#include <QString>
#include <vector>

class QTimer;

namespace OpenC3::ViewModels {

class PluginTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    // Named TargetCount, not Author, even though Plugin now carries a real
    // author field (from WorkspaceService's gemspec parsing) - this column's
    // header and data have always been the plugin's target count.
    enum Column { Name = 0, Version, Status, TargetCount, ColCount };

    explicit PluginTableModel(QObject* parent = nullptr);

    void setPlugins(std::vector<Models::Plugin> plugins);
    [[nodiscard]] const Models::Plugin* pluginAt(int row) const noexcept;

    [[nodiscard]] int      rowCount(const QModelIndex& = {})    const override;
    [[nodiscard]] int      columnCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role)   const override;
    [[nodiscard]] QVariant headerData(int, Qt::Orientation, int) const override;

private:
    std::vector<Models::Plugin> plugins_;
};

// ─────────────────────────────────────────────────────────────────────────────

/// Installed COSMOS instances, deliberately separate from PluginTableModel's
/// host source folders. The full instance id (including __<counter>) is the
/// only safe identifier for uninstall.
class InstalledPluginTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Instance = 0, Gem, Description, ColCount };

    explicit InstalledPluginTableModel(QObject* parent = nullptr);

    void setPlugins(std::vector<Models::Plugin> plugins);
    [[nodiscard]] const Models::Plugin* pluginAt(int row) const noexcept;

    [[nodiscard]] int      rowCount(const QModelIndex& = {}) const override;
    [[nodiscard]] int      columnCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role) const override;
    [[nodiscard]] QVariant headerData(int, Qt::Orientation, int) const override;

private:
    std::vector<Models::Plugin> plugins_;
};

// ─────────────────────────────────────────────────────────────────────────────

class PluginViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit PluginViewModel(
        Services::IWorkspaceService&    workspace,
        Services::ICosmosPluginService& cosmosPlugin,
        Services::IConnectionService&   connection,
        QObject*                        parent = nullptr);

    // Every action dispatches a worker that captures `this` and marshals its
    // result back with a queued invocation. Without waiting for that worker
    // here, destroying the ViewModel while one is in flight leaves the worker
    // dereferencing freed memory - a use-after-free that surfaced as an
    // intermittent crash once the ViewModel gained a test that constructs and
    // destroys it repeatedly.
    ~PluginViewModel() override;

    [[nodiscard]] PluginTableModel* pluginModel() const noexcept;
    [[nodiscard]] InstalledPluginTableModel* installedPluginModel() const noexcept;
    [[nodiscard]] bool              isBusy()         const noexcept;
    [[nodiscard]] QString           statusMessage()  const noexcept;

public slots:
    void refresh();
    void refreshInstalled();
    void install(const QString& gemFilePath);
    void buildAndInstall(const QString& pluginRootPath);
    void remove(const QString& pluginName);
    void validate(const QString& localPath);
    void build(const QString& pluginRootPath);

signals:
    void pluginListChanged();
    void installedPluginListChanged();
    void busyChanged();
    void statusMessageChanged();
    void validationComplete(bool valid, const QString& summary);
    void actionCompleted(const QString& pluginName, bool success);
    void jobStatusChanged(const QString& processName,
                          const QString& state,
                          const QString& detail,
                          bool finished,
                          bool successful);

private:
    [[nodiscard]] QString pluginRootPath() const noexcept;

    void setBusy(bool busy);
    void setStatus(const QString& msg);
    void startJobTracking(const QString& processName);
    void pollJobStatus();

    Services::IWorkspaceService&    workspace_;
    Services::ICosmosPluginService& cosmosPlugin_;
    Services::IConnectionService& connection_;
    PluginTableModel*             tableModel_{nullptr};
    InstalledPluginTableModel*    installedTableModel_{nullptr};

    // Handle on the in-flight worker so the destructor can wait for it. Only
    // one action runs at a time - every entry point returns early when busy_
    // is set - so a single future is sufficient.
    QFuture<void>             worker_;
    QFuture<void>             jobWorker_;
    QTimer*                   jobPollTimer_{nullptr};
    QString                   activeProcessName_;
    int                       jobPollAttempts_{0};
    int                       jobPollFailures_{0};
    bool                      jobPollInFlight_{false};

    bool                      busy_{false};
    QString                   statusMessage_;
};

} // namespace OpenC3::ViewModels
