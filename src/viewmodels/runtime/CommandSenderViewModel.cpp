#include "CommandSenderViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>

namespace OpenC3::ViewModels {

CommandSenderViewModel::CommandSenderViewModel(
    Services::ICosmosCommandService& commandService, QObject* parent)
    : ViewModelBase(parent)
    , commandService_(commandService)
{}

CommandSenderViewModel::~CommandSenderViewModel()
{
    if (worker_.isRunning())
        worker_.waitForFinished();
}

bool    CommandSenderViewModel::isBusy() const noexcept { return busy_; }
QString CommandSenderViewModel::statusMessage() const noexcept { return statusMessage_; }

void CommandSenderViewModel::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void CommandSenderViewModel::setStatus(const QString& msg)
{
    statusMessage_ = msg;
    emit statusMessageChanged();
}

void CommandSenderViewModel::send(
    const QString& targetName, const QString& commandName,
    const QVector<QPair<QString, QString>>& parameters)
{
    if (busy_) return;
    if (!commandService_.isAvailable()) {
        setStatus("Not connected to the COSMOS API.");
        emit commandCompleted(targetName, commandName, false,
                              "Not connected to the COSMOS API.");
        return;
    }
    if (targetName.trimmed().isEmpty() || commandName.trimmed().isEmpty()) {
        setStatus("Select a target and command first.");
        emit commandCompleted(targetName, commandName, false,
                              "Select a target and command first.");
        return;
    }

    // Snapshot exactly what will be sent so a later sendConfirmed() resends
    // this, not whatever the form happens to hold if the user edited it
    // while the hazardous prompt was open.
    pendingTarget_ = targetName;
    pendingCommand_ = commandName;
    pendingParameters_.clear();
    for (const auto& [name, value] : parameters)
        pendingParameters_.emplace_back(name.toStdString(), value.toStdString());

    const std::string target = targetName.toStdString();
    const std::string command = commandName.toStdString();
    const auto params = pendingParameters_;

    setBusy(true);
    setStatus("Sending " + targetName + " " + commandName);

    worker_ = QtConcurrent::run([this, target, command, params] {
        const auto result = commandService_.send(target, command, params);
        QMetaObject::invokeMethod(this,
            [this, result, qtarget = QString::fromStdString(target),
             qcommand = QString::fromStdString(command)] {
                setBusy(false);

                if (result.hazardous) {
                    setStatus("Hazardous command - confirmation required.");
                    emit hazardousConfirmationNeeded(
                        qtarget, qcommand,
                        QString::fromStdString(result.hazardousDescription));
                    return;
                }

                const QString detail = result.success
                    ? QString::fromStdString(result.cmdString)
                    : QString::fromStdString(result.errorMessage);
                setStatus(result.success
                              ? "Sent: " + detail
                              : "Command failed: " + detail);
                emit commandCompleted(qtarget, qcommand, result.success, detail);
            }, Qt::QueuedConnection);
    });
}

void CommandSenderViewModel::sendConfirmed()
{
    if (busy_) return;
    if (pendingTarget_.isEmpty() || pendingCommand_.isEmpty()) return;
    if (!commandService_.isAvailable()) {
        setStatus("Not connected to the COSMOS API.");
        emit commandCompleted(pendingTarget_, pendingCommand_, false,
                              "Not connected to the COSMOS API.");
        return;
    }

    const std::string target = pendingTarget_.toStdString();
    const std::string command = pendingCommand_.toStdString();
    const auto params = pendingParameters_;

    setBusy(true);
    setStatus("Sending " + pendingTarget_ + " " + pendingCommand_
              + " (hazardous check bypassed after confirmation)");

    worker_ = QtConcurrent::run([this, target, command, params] {
        const auto result = commandService_.sendConfirmed(target, command, params);
        QMetaObject::invokeMethod(this,
            [this, result, qtarget = QString::fromStdString(target),
             qcommand = QString::fromStdString(command)] {
                setBusy(false);
                const QString detail = result.success
                    ? QString::fromStdString(result.cmdString)
                    : QString::fromStdString(result.errorMessage);
                setStatus(result.success
                              ? "Sent: " + detail
                              : "Command failed: " + detail);
                emit commandCompleted(qtarget, qcommand, result.success, detail);
            }, Qt::QueuedConnection);
    });
}

} // namespace OpenC3::ViewModels
