#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/connection/IConnectionService.h"
#include "services/filesystem/IRemoteFileService.h"

#include <QStringList>
#include <QString>

namespace OpenC3::ViewModels {

class PacketToolsViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isConnected  READ isConnected  NOTIFY connectionChanged)
    Q_PROPERTY(bool    isBusy       READ isBusy       NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit PacketToolsViewModel(
        Services::IConnectionService& connection,
        Services::IRemoteFileService& fs,
        QObject*                      parent = nullptr);

    [[nodiscard]] bool    isConnected()   const noexcept;
    [[nodiscard]] bool    isBusy()        const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;

public slots:
    void refreshLogList();
    void openLogFile(const QString& remotePath);
    void analyzePackets(const QString& remotePath, const QString& filter);

signals:
    void connectionChanged();
    void busyChanged();
    void statusMessageChanged();
    void logListReady(const QStringList& files);
    void logContentReady(const QString& path, const QString& content);
    void analysisReady(const QString& summary);

private:
    void setBusy(bool busy);
    void setStatus(const QString& msg);

    Services::IConnectionService& connection_;
    Services::IRemoteFileService& fs_;
    bool                          busy_{false};
    QString                       status_;

    static constexpr const char* kLogDir = "/cosmos/outputs/logs/tlm/";
};

} // namespace OpenC3::ViewModels
