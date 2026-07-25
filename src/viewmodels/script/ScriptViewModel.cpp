#include "ScriptViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QTimer>

namespace OpenC3::ViewModels {

ScriptListModel::ScriptListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

void ScriptListModel::setScripts(std::vector<Models::CosmosScript> scripts)
{
    beginResetModel();
    scripts_ = std::move(scripts);
    endResetModel();
}

const Models::CosmosScript* ScriptListModel::scriptAt(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(scripts_.size())) return nullptr;
    return &scripts_[static_cast<std::size_t>(row)];
}

int ScriptListModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(scripts_.size());
}

QVariant ScriptListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(scripts_.size()))
        return {};
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
        return QString::fromStdString(scripts_[static_cast<std::size_t>(index.row())].path);
    return {};
}

ScriptViewModel::ScriptViewModel(
    Services::IScriptService& scriptService, QObject* parent)
    : ViewModelBase(parent)
    , scriptService_(scriptService)
    , listModel_(new ScriptListModel(this))
    , pollTimer_(new QTimer(this))
{
    pollTimer_->setInterval(1000);
    connect(pollTimer_, &QTimer::timeout, this, &ScriptViewModel::pollStatus);
}

ScriptViewModel::~ScriptViewModel()
{
    pollTimer_->stop();
    // Same lifetime contract as PluginViewModel: every worker captures
    // `this` and (for worker_/pollWorker_) posts a queued invocation back to
    // this object, so all three must finish before this object is freed.
    if (worker_.isRunning())
        worker_.waitForFinished();
    if (pollWorker_.isRunning())
        pollWorker_.waitForFinished();
    if (actionWorker_.isRunning())
        actionWorker_.waitForFinished();
}

ScriptListModel* ScriptViewModel::scriptModel() const noexcept { return listModel_; }
bool             ScriptViewModel::isBusy() const noexcept { return busy_; }
QString          ScriptViewModel::statusMessage() const noexcept { return statusMessage_; }
QString          ScriptViewModel::activeRunningScriptId() const noexcept { return activeRunningScriptId_; }

void ScriptViewModel::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void ScriptViewModel::setStatus(const QString& msg)
{
    statusMessage_ = msg;
    emit statusMessageChanged();
}

void ScriptViewModel::refresh(const QString& targetFilter)
{
    if (busy_) return;
    if (!scriptService_.isAvailable()) {
        listModel_->setScripts({});
        emit scriptListChanged();
        setStatus("Not connected to the COSMOS Script Runner API.");
        return;
    }

    setBusy(true);
    setStatus("Loading COSMOS scripts");
    const std::string filter = targetFilter.toStdString();
    worker_ = QtConcurrent::run([this, filter] {
        auto scripts = scriptService_.listScripts(filter);
        QMetaObject::invokeMethod(this,
            [this, list = std::move(scripts)]() mutable {
                listModel_->setScripts(std::move(list));
                emit scriptListChanged();
                setBusy(false);
                setStatus(QString("Loaded %1 script(s)").arg(listModel_->rowCount()));
            },
            Qt::QueuedConnection);
    });
}

void ScriptViewModel::run(const QString& scriptPath)
{
    if (busy_) return;
    if (!scriptService_.isAvailable()) {
        setStatus("Not connected to the COSMOS Script Runner API.");
        emit runStarted(scriptPath, false);
        return;
    }
    if (scriptPath.trimmed().isEmpty()) {
        setStatus("Select a script first.");
        emit runStarted(scriptPath, false);
        return;
    }

    const std::string path = scriptPath.toStdString();
    setBusy(true);
    setStatus("Starting " + scriptPath + " in COSMOS Script Runner (disconnected mode)");

    worker_ = QtConcurrent::run([this, path] {
        const auto result = scriptService_.run(path);
        QMetaObject::invokeMethod(this,
            [this, result, qpath = QString::fromStdString(path)] {
                setBusy(false);
                const QString outcome = result.success
                    ? QString("COSMOS started running %1 (id %2).")
                          .arg(qpath, QString::fromStdString(result.runningScriptId))
                    : QString::fromStdString("Run failed: " + result.message);
                setStatus(outcome);
                emit runStarted(qpath, result.success);
                if (result.success)
                    startTracking(QString::fromStdString(result.runningScriptId));
            }, Qt::QueuedConnection);
    });
}

void ScriptViewModel::stop()
{
    if (activeRunningScriptId_.isEmpty()) return;
    const std::string id = activeRunningScriptId_.toStdString();
    actionWorker_ = QtConcurrent::run([this, id] { (void)scriptService_.stop(id); });
}

void ScriptViewModel::pause()
{
    if (activeRunningScriptId_.isEmpty()) return;
    const std::string id = activeRunningScriptId_.toStdString();
    actionWorker_ = QtConcurrent::run([this, id] { (void)scriptService_.pause(id); });
}

void ScriptViewModel::resume()
{
    if (activeRunningScriptId_.isEmpty()) return;
    const std::string id = activeRunningScriptId_.toStdString();
    actionWorker_ = QtConcurrent::run([this, id] { (void)scriptService_.resume(id); });
}

void ScriptViewModel::startTracking(const QString& runningScriptId)
{
    if (runningScriptId.trimmed().isEmpty()) return;

    activeRunningScriptId_ = runningScriptId.trimmed();
    pollAttempts_ = 0;
    pollFailures_ = 0;
    pollInFlight_ = false;
    emit scriptStatusChanged(activeRunningScriptId_, "spawning",
                             "Waiting for COSMOS Script Runner", false, false);
    pollTimer_->start();
    pollStatus();
}

void ScriptViewModel::pollStatus()
{
    constexpr int kMaxPollAttempts = 3600; // one hour at a one-second interval -
                                            // scripts can legitimately run far
                                            // longer than a plugin install job
    if (pollInFlight_ || activeRunningScriptId_.isEmpty()) return;

    if (pollAttempts_ >= kMaxPollAttempts) {
        const QString id = activeRunningScriptId_;
        activeRunningScriptId_.clear();
        pollTimer_->stop();
        emit scriptStatusChanged(id, "Timed out",
            "Stopped polling after one hour without a terminal state.", true, false);
        return;
    }

    pollInFlight_ = true;
    const QString id = activeRunningScriptId_;
    pollWorker_ = QtConcurrent::run([this, id] {
        const auto result = scriptService_.status(id.toStdString());
        QMetaObject::invokeMethod(this,
            [this, id, result] {
                pollInFlight_ = false;
                if (id != activeRunningScriptId_) return;
                ++pollAttempts_;

                if (!result.querySucceeded) {
                    ++pollFailures_;
                    if (pollFailures_ >= 5) {
                        activeRunningScriptId_.clear();
                        pollTimer_->stop();
                        emit scriptStatusChanged(id, "Status unavailable",
                            QString::fromStdString(result.errorMessage), true, false);
                        return;
                    }
                    emit scriptStatusChanged(id, "Status unavailable",
                        QString::fromStdString(result.errorMessage), false, false);
                    return;
                }
                pollFailures_ = 0;

                if (!result.found) {
                    // ScriptStatusModel entries expire; a 404 after the
                    // script was previously seen running means it is gone
                    // for good, not merely "not created yet" - unlike the
                    // plugin install path there is no separate "starting"
                    // window here since run() only returns after COSMOS
                    // has already spawned the process and created the model.
                    activeRunningScriptId_.clear();
                    pollTimer_->stop();
                    emit scriptStatusChanged(id, "Unknown",
                        "COSMOS no longer reports a status for this script.",
                        true, false);
                    return;
                }

                QString detail = QString::fromStdString(result.currentFilename);
                if (detail.isEmpty())
                    detail = QString::fromStdString(result.filename);
                if (result.lineNo > 0)
                    detail += QString(" (line %1)").arg(result.lineNo);
                const QString output = QString::fromStdString(result.output).trimmed();
                if (!output.isEmpty())
                    detail += (detail.isEmpty() ? "" : "\n") + output;

                if (!result.isTerminal()) {
                    emit scriptStatusChanged(id,
                        QString::fromStdString(result.state), detail, false, false);
                    return;
                }

                activeRunningScriptId_.clear();
                pollTimer_->stop();
                emit scriptStatusChanged(id,
                    QString::fromStdString(result.state), detail, true,
                    result.completedSuccessfully());
            },
            Qt::QueuedConnection);
    });
}

} // namespace OpenC3::ViewModels
