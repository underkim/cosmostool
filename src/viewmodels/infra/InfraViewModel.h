#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/connection/IConnectionService.h"
#include "services/filesystem/IRemoteFileService.h"

#include <QString>

namespace OpenC3::ViewModels {

class InfraViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isConnected   READ isConnected   NOTIFY connectionChanged)
    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    static constexpr const char* kDefaultEnvPath     = "/cosmos/.env";
    static constexpr const char* kDefaultComposePath = "/cosmos/compose.yaml";

    explicit InfraViewModel(
        Services::IConnectionService& connection,
        Services::IRemoteFileService& fs,
        QObject*                      parent = nullptr);

    [[nodiscard]] bool    isConnected()   const noexcept;
    [[nodiscard]] bool    isBusy()        const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;

public slots:
    void loadEnvFile(const QString& remotePath);
    void saveEnvFile(const QString& remotePath, const QString& content);
    void loadComposeFile(const QString& remotePath);
    void saveComposeFile(const QString& remotePath, const QString& content);

    // Generates a unified diff patch between originalContent and currentContent.
    // Result is emitted via patchReady(). Falls back to simple line diff if git unavailable.
    void generatePatch(const QString& originalContent,
                       const QString& currentContent,
                       const QString& filename);

    // Creates an OpenC3 plugin scaffold directory tree on the remote host.
    // templateType: 0=Generic, 1=Satellite Target, 2=GSE Interface, 3=Tool
    void scaffoldPlugin(
        const QString& remoteRoot,
        const QString& pluginName,
        const QString& targetName,
        const QString& pluginNamespace,
        const QString& description,
        int            templateType);

signals:
    void connectionChanged();
    void busyChanged();
    void statusMessageChanged();
    void envLoaded(const QString& path, const QString& content);
    void composeLoaded(const QString& path, const QString& content);
    void fileSaved(const QString& path, bool success);
    void patchReady(const QString& patchContent);
    void scaffoldComplete(const QString& rootPath, bool success, const QString& detail);

private:
    void setStatus(const QString& msg);
    void setBusy(bool b);

    // Builds the full set of scaffold file contents for a plugin.
    // Returns a map of relativePath → fileContent.
    static QMap<QString, QString> buildScaffoldFiles(
        const QString& pluginName,
        const QString& targetName,
        const QString& ns,
        const QString& description,
        int            templateType);

    // Local unified diff (git diff --no-index preferred; line diff as fallback).
    static QString computePatch(
        const QString& original,
        const QString& modified,
        const QString& filenameA,
        const QString& filenameB);

    Services::IConnectionService& connection_;
    Services::IRemoteFileService& fs_;
    bool    connected_{false};
    bool    busy_{false};
    QString statusMessage_;
};

} // namespace OpenC3::ViewModels
