#pragma once

#include "viewmodels/base/ViewModelBase.h"
#include "services/runtime/ICosmosRuntimeService.h"
#include "models/Runtime.h"

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QFuture>
#include <QString>
#include <vector>

class QTimer;

namespace OpenC3::ViewModels {

/// Target names in the connected scope, read through ICosmosRuntimeService -
/// COSMOS logical targets (TargetModel), never Workspace source folders or
/// Installed plugin instance names.
class TargetListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit TargetListModel(QObject* parent = nullptr);

    void setTargets(std::vector<Models::RuntimeTarget> targets);
    [[nodiscard]] const Models::RuntimeTarget* targetAt(int row) const noexcept;

    [[nodiscard]] int      rowCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role) const override;

private:
    std::vector<Models::RuntimeTarget> targets_;
};

/// Command or telemetry packet names under the selected target. The same
/// model backs both lists in the UI; PacketRunnerViewModel tracks which kind
/// (command vs telemetry) is currently loaded so a stale selection from one
/// list is never misread as the other.
class PacketListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit PacketListModel(QObject* parent = nullptr);

    void setPackets(std::vector<Models::RuntimePacket> packets);
    [[nodiscard]] const Models::RuntimePacket* packetAt(int row) const noexcept;

    [[nodiscard]] int      rowCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role) const override;

private:
    std::vector<Models::RuntimePacket> packets_;
};

/// Current item values for one telemetry packet.
class TelemetryValueTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Item = 0, Value, LimitsState, ColCount };

    explicit TelemetryValueTableModel(QObject* parent = nullptr);

    void setValues(std::vector<Models::TelemetryItemValue> values);

    [[nodiscard]] int      rowCount(const QModelIndex& = {}) const override;
    [[nodiscard]] int      columnCount(const QModelIndex& = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex&, int role) const override;
    [[nodiscard]] QVariant headerData(int, Qt::Orientation, int) const override;

private:
    std::vector<Models::TelemetryItemValue> values_;
};

/// Drives the read-only COSMOS target/command/telemetry browser: lists
/// targets, lists command/telemetry packets under a selected target, and
/// polls current telemetry values for a selected packet. Async plumbing
/// mirrors PluginViewModel/ScriptViewModel: QtConcurrent::run + queued
/// invocation back to the GUI thread, worker futures waited on in the
/// destructor so a worker's captured `this` never outlives the object.
class RuntimeViewModel final : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(bool    isBusy        READ isBusy        NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit RuntimeViewModel(
        Services::ICosmosRuntimeService& runtime,
        QObject*                         parent = nullptr);

    ~RuntimeViewModel() override;

    [[nodiscard]] TargetListModel*          targetModel()    const noexcept;
    [[nodiscard]] PacketListModel*          packetModel()    const noexcept;
    [[nodiscard]] TelemetryValueTableModel* telemetryModel() const noexcept;
    [[nodiscard]] bool                      isBusy()         const noexcept;
    [[nodiscard]] QString                   statusMessage()  const noexcept;

public slots:
    void refreshTargets();
    void loadCommands(const QString& targetName);
    void loadTelemetry(const QString& targetName);
    void refreshTelemetryValues(const QString& targetName, const QString& packetName);
    /// Starts (or stops, when targetName/packetName are empty) automatic
    /// telemetry polling at a fixed 1s interval - separate slot from
    /// refreshTelemetryValues() so the UI can distinguish "poll this
    /// continuously" from "just refresh once".
    void setTelemetryPolling(const QString& targetName, const QString& packetName);

signals:
    void targetListChanged();
    void packetListChanged();
    void telemetryValuesChanged();
    void busyChanged();
    void statusMessageChanged();

private:
    void setBusy(bool busy);
    void setStatus(const QString& msg);

    Services::ICosmosRuntimeService& runtime_;
    TargetListModel*                 targetModel_{nullptr};
    PacketListModel*                 packetModel_{nullptr};
    TelemetryValueTableModel*        telemetryModel_{nullptr};

    QFuture<void> worker_;
    QFuture<void> pollWorker_;
    QTimer*       pollTimer_{nullptr};
    QString       pollTarget_;
    QString       pollPacket_;
    bool          pollInFlight_{false};

    bool    busy_{false};
    QString statusMessage_;
};

} // namespace OpenC3::ViewModels
