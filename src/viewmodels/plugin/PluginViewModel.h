#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/plugin/IPluginService.h"
#include "services/connection/IConnectionService.h"
#include "models/Plugin.h"

#include <QAbstractTableModel>
#include <QString>
#include <vector>

namespace OpenC3::ViewModels {

class PluginTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    // Named TargetCount, not Author, even though Plugin now carries a real
    // author field (from PluginService's gemspec parsing) - this column's
    // header and data have always been the plugin's target count.
    enum Column { Name = 0, Version, Status, TargetCount, ColCount };

    explicit PluginTableModel(QObject* parent = nullptr);

    void setPlugins(std::vector<Models::Plugin> plugins);
    [[nodiscard]] const Models::Plugin* pluginAt(int row) const noexcept;

    [[nodiscard]] int      rowCount(const QModelIndex& = {})    const override;
    [[nodiscard]] int      columnCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role)   const override;
    [[nodiscard]] QVariant headerData(int, Qt::Orientation, int) const override;

private:
    std::vector<Models::Plugin> plugins_;
};

// ─────────────────────────────────────────────────────────────────────────────

class PluginViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit PluginViewModel(
        Services::IPluginService&     plugin,
        Services::IConnectionService& connection,
        QObject*                      parent = nullptr);

    [[nodiscard]] PluginTableModel* pluginModel()    const noexcept;
    [[nodiscard]] bool              isBusy()         const noexcept;
    [[nodiscard]] QString           statusMessage()  const noexcept;

public slots:
    void refresh();
    void install(const QString& gemFilePath);
    void remove(const QString& pluginName);
    void validate(const QString& localPath);
    void build(const QString& pluginRootPath);

signals:
    void pluginListChanged();
    void busyChanged();
    void statusMessageChanged();
    void validationComplete(bool valid, const QString& summary);
    void actionCompleted(const QString& pluginName, bool success);

private:
    [[nodiscard]] QString pluginRootPath() const noexcept;

    void setBusy(bool busy);
    void setStatus(const QString& msg);

    Services::IPluginService&     plugin_;
    Services::IConnectionService& connection_;
    PluginTableModel*             tableModel_{nullptr};
    bool                      busy_{false};
    QString                   statusMessage_;
};

} // namespace OpenC3::ViewModels
