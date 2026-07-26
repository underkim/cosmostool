#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/runtime/ICosmosCommandService.h"
#include "models/Command.h"

#include <QFuture>
#include <QString>
#include <QVector>
#include <utility>

namespace OpenC3::ViewModels {

/// Drives command sending against a running COSMOS: a distinct ViewModel
/// from RuntimeViewModel (which stays read-only - see
/// ICosmosCommandService's header for why) so a view that never wires this
/// one up structurally cannot send a command by accident.
///
/// The hazardous-confirmation flow deliberately keeps control in the caller:
/// send() reports back "this needs confirmation" through
/// hazardousConfirmationNeeded rather than popping its own dialog, so the
/// View decides how to ask (and can log/audit the decision) and then calls
/// sendConfirmed() only after the user has explicitly agreed - mirroring
/// COSMOS's own Script prompt_for_hazardous flow rather than reinventing it.
class CommandSenderViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit CommandSenderViewModel(
        Services::ICosmosCommandService& commandService,
        QObject*                         parent = nullptr);

    ~CommandSenderViewModel() override;

    [[nodiscard]] bool    isBusy()        const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;

public slots:
    /// Sends with full range and hazardous checks. `parameters` are
    /// (name, rawTextValue) pairs; the service decides numeric-vs-string
    /// encoding the same way COSMOS's own text syntax would.
    void send(const QString& targetName, const QString& commandName,
              const QVector<QPair<QString, QString>>& parameters);

    /// Re-sends the last send() call bypassing only the hazardous check -
    /// must only be invoked after the user has explicitly confirmed a
    /// hazardousConfirmationNeeded signal for that exact command.
    void sendConfirmed();

signals:
    void busyChanged();
    void statusMessageChanged();

    /// A send() attempt reached a definitive outcome (success or a
    /// non-hazardous failure) - success is true only when COSMOS accepted
    /// and returned its formatted command string.
    void commandCompleted(const QString& targetName, const QString& commandName,
                          bool success, const QString& detail);

    /// COSMOS rejected the command pending hazardous confirmation. The View
    /// must ask the user and call sendConfirmed() to actually send it, or
    /// simply do nothing to cancel - state for the pending resend is kept
    /// here, not re-derived from the description text.
    void hazardousConfirmationNeeded(const QString& targetName,
                                     const QString& commandName,
                                     const QString& description);

private:
    void setBusy(bool busy);
    void setStatus(const QString& msg);

    Services::ICosmosCommandService& commandService_;

    QFuture<void> worker_;

    // Snapshot of the pending send, kept so sendConfirmed() resends the
    // *exact* same command the user was warned about - never re-reads
    // whatever the View's current form fields happen to hold by then.
    QString pendingTarget_;
    QString pendingCommand_;
    std::vector<Services::CommandParameter> pendingParameters_;

    bool    busy_{false};
    QString statusMessage_;
};

} // namespace OpenC3::ViewModels
