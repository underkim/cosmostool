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
    // Derived paths — call these after connection to get live cosmos root
    [[nodiscard]] QString cosmosRootPath()   const noexcept;
    [[nodiscard]] QString defaultEnvPath()   const noexcept;
    [[nodiscard]] QString defaultComposePath() const noexcept;
    [[nodiscard]] QString defaultPluginsPath() const noexcept;

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

    // Volume override workflow:
    //   1. loadContainers()         → populates container list via docker ps
    //   2. extractContainerFile()   → docker exec cat → containerFileExtracted()
    //   3. applyVolumeOverride()    → saves file to host path → volumeEntryReady()
    void loadContainers();
    void extractContainerFile(const QString& container, const QString& filePath);
    void applyVolumeOverride(const QString& container,
                              const QString& containerFilePath,
                              const QString& hostSavePath,
                              const QString& content);

    // Creates an OpenC3 plugin scaffold directory tree on the remote host.
    // templateType: 0=Generic, 1=Satellite Target, 2=GSE Interface, 3=Tool
    void scaffoldPlugin(
        const QString& remoteRoot,
        const QString& pluginName,
        const QString& targetName,
        const QString& pluginNamespace,
        const QString& description,
        int            templateType);

    // Appends a new target directory tree to an existing plugin root on the remote.
    void addTargetToPlugin(
        const QString& pluginRoot,   // e.g. /cosmos/plugins/cosmos-my-plugin
        const QString& targetName,
        const QString& pluginNamespace,
        int            templateType);

signals:
    void connectionChanged();
    void busyChanged();
    void statusMessageChanged();
    void envLoaded(const QString& path, const QString& content);
    void composeLoaded(const QString& path, const QString& content);
    void fileSaved(const QString& path, bool success);
    void containersLoaded(const QStringList& names);
    void containerFileExtracted(const QString& containerPath, const QString& content);
    void volumeEntryReady(const QString& composeEntry);
    void overrideApplied(bool ok, const QString& hostPath);
    void scaffoldComplete(const QString& rootPath, bool success, const QString& detail);
    void targetAdded(const QString& targetName, bool success, const QString& detail);

private:
    [[nodiscard]] QString infraRootPath() const noexcept;

    void setStatus(const QString& msg);
    void setBusy(bool b);

    Services::IConnectionService& connection_;
    Services::IRemoteFileService& fs_;
    bool    connected_{false};
    bool    busy_{false};
    QString statusMessage_;
};

} // namespace OpenC3::ViewModels
