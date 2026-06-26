#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/connection/IConnectionService.h"
#include "services/filesystem/IRemoteFileService.h"

#include <QAtomicInt>
#include <QString>
#include <memory>

namespace OpenC3::ViewModels {

class LogViewerViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isConnected   READ isConnected   NOTIFY connectionChanged)
    Q_PROPERTY(bool    isStreaming   READ isStreaming   NOTIFY streamingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit LogViewerViewModel(
        Services::IConnectionService& connection,
        Services::IRemoteFileService& fs,
        QObject*                      parent = nullptr);

    ~LogViewerViewModel() override;

    [[nodiscard]] bool    isConnected()   const noexcept;
    [[nodiscard]] bool    isStreaming()   const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;

public slots:
    void startStream(const QString& command);
    void stopStream();

signals:
    void connectionChanged();
    void streamingChanged();
    void statusMessageChanged();
    void logLineReceived(const QString& line);
    void streamEnded();

private:
    void setStreaming(bool s);
    void setStatus(const QString& msg);

    Services::IConnectionService& connection_;
    Services::IRemoteFileService& fs_;
    bool                          streaming_{false};
    QString                       status_;
    QAtomicInt                    stopFlag_{0};
};

} // namespace OpenC3::ViewModels
