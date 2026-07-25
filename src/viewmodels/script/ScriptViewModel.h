#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/script/IScriptService.h"
#include "models/Script.h"

#include <QAbstractListModel>
#include <QFuture>
#include <QString>
#include <vector>

class QTimer;

namespace OpenC3::ViewModels {

/// Scripts deployed in the connected scope, read through IScriptService -
/// COSMOS bucket-stored scripts, never Workspace source folders or the
/// host-shell "Run Script" file list PluginView already has.
class ScriptListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ScriptListModel(QObject* parent = nullptr);

    void setScripts(std::vector<Models::CosmosScript> scripts);
    [[nodiscard]] const Models::CosmosScript* scriptAt(int row) const noexcept;

    [[nodiscard]] int      rowCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role) const override;

private:
    std::vector<Models::CosmosScript> scripts_;
};

/// Drives COSMOS Script Runner: lists deployed scripts, starts one
/// disconnected (no interactive prompt/breakpoint support - see
/// IScriptService::run()), and polls OpenC3::ScriptStatusModel through to a
/// terminal state exactly like PluginViewModel does for plugin install/
/// uninstall jobs (same QTimer + QFuture<void> + failure-count-limited
/// polling pattern, deliberately kept consistent across both ViewModels).
class ScriptViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit ScriptViewModel(
        Services::IScriptService& scriptService,
        QObject*                  parent = nullptr);

    // Mirrors PluginViewModel's destructor contract: every action dispatches
    // a worker capturing `this`, so both the action worker and the poll
    // worker must be waited on before this object is destroyed.
    ~ScriptViewModel() override;

    [[nodiscard]] ScriptListModel* scriptModel() const noexcept;
    [[nodiscard]] bool             isBusy()        const noexcept;
    [[nodiscard]] QString          statusMessage()  const noexcept;
    [[nodiscard]] QString          activeRunningScriptId() const noexcept;

public slots:
    void refresh(const QString& targetFilter = {});
    void run(const QString& scriptPath);
    void stop();
    void pause();
    void resume();

signals:
    void scriptListChanged();
    void busyChanged();
    void statusMessageChanged();
    void runStarted(const QString& scriptPath, bool success);
    void scriptStatusChanged(const QString& runningScriptId,
                             const QString& state,
                             const QString& detail,
                             bool           finished,
                             bool           successful);

private:
    void setBusy(bool busy);
    void setStatus(const QString& msg);
    void startTracking(const QString& runningScriptId);
    void pollStatus();

    Services::IScriptService& scriptService_;
    ScriptListModel*          listModel_{nullptr};

    QFuture<void> worker_;
    QFuture<void> pollWorker_;
    QFuture<void> actionWorker_;  ///< stop()/pause()/resume() - fire-and-forget
                                   ///< but still captures `this`, so it must be
                                   ///< waited on in the destructor too.
    QTimer*       pollTimer_{nullptr};
    QString       activeRunningScriptId_;
    int           pollAttempts_{0};
    int           pollFailures_{0};
    bool          pollInFlight_{false};

    bool    busy_{false};
    QString statusMessage_;
};

} // namespace OpenC3::ViewModels
