#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "viewmodels/cmdtlm/CmdTlmParser.h"
#include "services/connection/IConnectionService.h"
#include "services/filesystem/IRemoteFileService.h"

#include <QStringList>
#include <QString>

namespace OpenC3::ViewModels {

class CmdTlmViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isConnected   READ isConnected   NOTIFY connectionChanged)
    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit CmdTlmViewModel(
        Services::IConnectionService&  connection,
        Services::IRemoteFileService&  fs,
        QObject*                       parent = nullptr);

    [[nodiscard]] bool    isConnected()   const noexcept;
    [[nodiscard]] bool    isBusy()        const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;

    // Default browse path derived from connection's cosmos root
    [[nodiscard]] QString defaultCmdTlmPath() const;

public slots:
    void listDirectory(const QString& remotePath);
    void listPluginFiles(const QString& pluginRootPath);
    void openFile(const QString& remotePath);
    void saveFile(const QString& remotePath, const QString& content);

    // Parse the given content and emit fileParsed() with the result.
    // Call this whenever the editor content changes or on-demand validation.
    void parseContent(const QString& content, const QString& filePath);

signals:
    void connectionChanged();
    void busyChanged();
    void statusMessageChanged();
    void directoryListed(const QStringList& entries, const QString& path);
    void pluginFilesListed(const QStringList& files, const QString& pluginRootPath);
    void fileOpened(const QString& path, const QString& content);
    void fileSaved(const QString& path, bool success);
    void fileParsed(const CmdTlmParseResult& result, const QString& filePath);

private:
    void setBusy(bool busy);
    void setStatus(const QString& msg);

    Services::IConnectionService& connection_;
    Services::IRemoteFileService& fs_;
    bool                          busy_{false};
    QString                       status_;
};

} // namespace OpenC3::ViewModels
