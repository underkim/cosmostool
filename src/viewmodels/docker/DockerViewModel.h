#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/connection/IConnectionService.h"
#include "services/docker/IDockerService.h"
#include "models/DockerContainer.h"

#include <QAbstractTableModel>
#include <QTimer>
#include <memory>
#include <vector>

namespace OpenC3::ViewModels {

/// Table model for the container list — follows Qt's Model/View pattern.
/// DockerViewModel owns this and exposes it to the view.
class ContainerTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Name = 0, Image, Status, Ports, ColCount };

    explicit ContainerTableModel(QObject* parent = nullptr);

    void setContainers(std::vector<Models::DockerContainer> containers);

    [[nodiscard]] int      rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int      columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation, int role) const override;

    [[nodiscard]] const Models::DockerContainer* containerAt(int row) const noexcept;

private:
    std::vector<Models::DockerContainer> containers_;
};

// ─────────────────────────────────────────────────────────────────────────────

class DockerViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(QString dockerVersion READ dockerVersion NOTIFY dockerVersionChanged)

public:
    explicit DockerViewModel(
        Services::IConnectionService& connection,
        Services::IDockerService&     docker,
        QObject*                      parent = nullptr);

    [[nodiscard]] ContainerTableModel* containerModel() const noexcept;
    [[nodiscard]] QString              dockerVersion()  const noexcept;

public slots:
    void refresh();
    void startContainer  (const QString& nameOrId);
    void stopContainer   (const QString& nameOrId);
    void restartContainer(const QString& nameOrId);
    void removeContainer (const QString& nameOrId);
    void fetchLogs(const QString& nameOrId, int tail = 200);

signals:
    void dockerVersionChanged();
    void logsReady(const QString& nameOrId, const QString& logs);
    void containerActionCompleted(const QString& nameOrId, bool success);

private:
    Services::IConnectionService& connection_;
    Services::IDockerService&     docker_;
    ContainerTableModel*          containerModel_{nullptr};
    QTimer*                       refreshTimer_{nullptr};
    QString                       dockerVersion_;
};

} // namespace OpenC3::ViewModels
