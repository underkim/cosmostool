#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "viewmodels/packettools/PacketSimulator.h"
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
    Q_PROPERTY(bool    simulatorRunning READ simulatorRunning NOTIFY simulatorStateChanged)

public:
    explicit PacketToolsViewModel(
        Services::IConnectionService& connection,
        Services::IRemoteFileService& fs,
        QObject*                      parent = nullptr);

    [[nodiscard]] bool    isConnected()   const noexcept;
    [[nodiscard]] bool    isBusy()        const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;
    [[nodiscard]] bool    simulatorRunning() const noexcept;
    [[nodiscard]] int     tcpClientCount()  const noexcept;

public slots:
    void refreshLogList();
    void openLogFile(const QString& remotePath);
    void analyzePackets(const QString& remotePath, const QString& filter);
    void startUdpSimulator(const QString& bindAddress, quint16 port);
    void startTcpSimulator(const QString& bindAddress, quint16 port);
    void stopSimulator();
    void sendUdpSimulatorPacket(const QString& host, quint16 port, const QString& hexPayload);
    void sendTcpSimulatorPacket(const QString& hexPayload);

signals:
    void connectionChanged();
    void busyChanged();
    void statusMessageChanged();
    void logListReady(const QStringList& files);
    void logContentReady(const QString& path, const QString& content);
    void analysisReady(const QString& summary);
    void simulatorStateChanged();
    void tcpClientCountChanged(int count);
    void simulatorError(const QString& reason);
    void simulatorPacketReceived(const QString& transport,
                                 const QString& peer,
                                 const QString& hexPayload,
                                 const QString& asciiPreview);
    void simulatorPacketSent(const QString& transport,
                             const QString& peer,
                             const QString& hexPayload);

private:
    void setBusy(bool busy);
    void setStatus(const QString& msg);

    Services::IConnectionService& connection_;
    Services::IRemoteFileService& fs_;
    bool                          busy_{false};
    PacketSimulator               simulator_;
    QString                       status_;

    static constexpr const char* kLogDir = "/cosmos/outputs/logs/tlm/";
};

} // namespace OpenC3::ViewModels
