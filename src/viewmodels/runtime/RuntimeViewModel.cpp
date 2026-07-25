#include "RuntimeViewModel.h"
#include "core/logging/Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QColor>
#include <QMetaObject>
#include <QTimer>

namespace OpenC3::ViewModels {

// ── TargetListModel ──────────────────────────────────────────────────────────

TargetListModel::TargetListModel(QObject* parent) : QAbstractListModel(parent) {}

void TargetListModel::setTargets(std::vector<Models::RuntimeTarget> targets)
{
    beginResetModel();
    targets_ = std::move(targets);
    endResetModel();
}

const Models::RuntimeTarget* TargetListModel::targetAt(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(targets_.size())) return nullptr;
    return &targets_[static_cast<std::size_t>(row)];
}

int TargetListModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(targets_.size());
}

QVariant TargetListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(targets_.size()))
        return {};
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
        return QString::fromStdString(targets_[static_cast<std::size_t>(index.row())].name);
    return {};
}

// ── PacketListModel ──────────────────────────────────────────────────────────

PacketListModel::PacketListModel(QObject* parent) : QAbstractListModel(parent) {}

void PacketListModel::setPackets(std::vector<Models::RuntimePacket> packets)
{
    beginResetModel();
    packets_ = std::move(packets);
    endResetModel();
}

const Models::RuntimePacket* PacketListModel::packetAt(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(packets_.size())) return nullptr;
    return &packets_[static_cast<std::size_t>(row)];
}

int PacketListModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(packets_.size());
}

QVariant PacketListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(packets_.size()))
        return {};
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
        return QString::fromStdString(packets_[static_cast<std::size_t>(index.row())].packetName);
    return {};
}

// ── TelemetryValueTableModel ─────────────────────────────────────────────────

TelemetryValueTableModel::TelemetryValueTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void TelemetryValueTableModel::setValues(std::vector<Models::TelemetryItemValue> values)
{
    beginResetModel();
    values_ = std::move(values);
    endResetModel();
}

int TelemetryValueTableModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(values_.size());
}

int TelemetryValueTableModel::columnCount(const QModelIndex&) const
{
    return ColCount;
}

QVariant TelemetryValueTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(values_.size()))
        return {};

    const auto& v = values_[static_cast<std::size_t>(index.row())];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Item:        return QString::fromStdString(v.itemName);
        case Value:        return QString::fromStdString(v.value);
        case LimitsState:  return QString::fromStdString(v.limitsState);
        default:           return {};
        }
    }
    if (role == Qt::ForegroundRole && index.column() == LimitsState) {
        if (v.limitsState.find("RED") != std::string::npos)
            return QColor(0xf4, 0x47, 0x47);
        if (v.limitsState.find("YELLOW") != std::string::npos)
            return QColor(0xce, 0x91, 0x78);
        if (v.limitsState.find("GREEN") != std::string::npos)
            return QColor(0x4e, 0xc9, 0xb0);
    }
    return {};
}

QVariant TelemetryValueTableModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case Item:        return "Item";
    case Value:        return "Value";
    case LimitsState:  return "Limits";
    default:           return {};
    }
}

// ── RuntimeViewModel ─────────────────────────────────────────────────────────

RuntimeViewModel::RuntimeViewModel(
    Services::ICosmosRuntimeService& runtime, QObject* parent)
    : ViewModelBase(parent)
    , runtime_(runtime)
    , targetModel_(new TargetListModel(this))
    , packetModel_(new PacketListModel(this))
    , telemetryModel_(new TelemetryValueTableModel(this))
    , pollTimer_(new QTimer(this))
{
    pollTimer_->setInterval(1000);
    connect(pollTimer_, &QTimer::timeout, this, [this] {
        if (!pollTarget_.isEmpty() && !pollPacket_.isEmpty())
            refreshTelemetryValues(pollTarget_, pollPacket_);
    });
}

RuntimeViewModel::~RuntimeViewModel()
{
    pollTimer_->stop();
    if (worker_.isRunning())
        worker_.waitForFinished();
    if (pollWorker_.isRunning())
        pollWorker_.waitForFinished();
}

TargetListModel*          RuntimeViewModel::targetModel()    const noexcept { return targetModel_; }
PacketListModel*          RuntimeViewModel::packetModel()    const noexcept { return packetModel_; }
TelemetryValueTableModel* RuntimeViewModel::telemetryModel() const noexcept { return telemetryModel_; }
bool                      RuntimeViewModel::isBusy()          const noexcept { return busy_; }
QString                   RuntimeViewModel::statusMessage()   const noexcept { return statusMessage_; }

void RuntimeViewModel::setBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    emit busyChanged();
}

void RuntimeViewModel::setStatus(const QString& msg)
{
    statusMessage_ = msg;
    emit statusMessageChanged();
}

void RuntimeViewModel::refreshTargets()
{
    if (busy_) return;
    if (!runtime_.isAvailable()) {
        targetModel_->setTargets({});
        emit targetListChanged();
        setStatus("Not connected to the COSMOS API.");
        return;
    }

    setBusy(true);
    setStatus("Loading COSMOS targets");
    worker_ = QtConcurrent::run([this] {
        auto targets = runtime_.listTargets();
        QMetaObject::invokeMethod(this,
            [this, list = std::move(targets)]() mutable {
                targetModel_->setTargets(std::move(list));
                emit targetListChanged();
                setBusy(false);
                setStatus(QString("Loaded %1 target(s)").arg(targetModel_->rowCount()));
            },
            Qt::QueuedConnection);
    });
}

void RuntimeViewModel::loadCommands(const QString& targetName)
{
    if (busy_) return;
    if (!runtime_.isAvailable()) {
        packetModel_->setPackets({});
        emit packetListChanged();
        setStatus("Not connected to the COSMOS API.");
        return;
    }
    if (targetName.trimmed().isEmpty()) return;

    setBusy(true);
    setStatus("Loading commands for " + targetName);
    const std::string target = targetName.toStdString();
    worker_ = QtConcurrent::run([this, target] {
        auto packets = runtime_.listCommands(target);
        QMetaObject::invokeMethod(this,
            [this, list = std::move(packets)]() mutable {
                packetModel_->setPackets(std::move(list));
                emit packetListChanged();
                setBusy(false);
                setStatus(QString("Loaded %1 command(s)").arg(packetModel_->rowCount()));
            },
            Qt::QueuedConnection);
    });
}

void RuntimeViewModel::loadTelemetry(const QString& targetName)
{
    if (busy_) return;
    if (!runtime_.isAvailable()) {
        packetModel_->setPackets({});
        emit packetListChanged();
        setStatus("Not connected to the COSMOS API.");
        return;
    }
    if (targetName.trimmed().isEmpty()) return;

    setBusy(true);
    setStatus("Loading telemetry packets for " + targetName);
    const std::string target = targetName.toStdString();
    worker_ = QtConcurrent::run([this, target] {
        auto packets = runtime_.listTelemetry(target);
        QMetaObject::invokeMethod(this,
            [this, list = std::move(packets)]() mutable {
                packetModel_->setPackets(std::move(list));
                emit packetListChanged();
                setBusy(false);
                setStatus(QString("Loaded %1 telemetry packet(s)").arg(packetModel_->rowCount()));
            },
            Qt::QueuedConnection);
    });
}

void RuntimeViewModel::refreshTelemetryValues(
    const QString& targetName, const QString& packetName)
{
    if (!runtime_.isAvailable()) {
        setStatus("Not connected to the COSMOS API.");
        return;
    }
    if (targetName.trimmed().isEmpty() || packetName.trimmed().isEmpty()) return;
    // Polling reuses this same call every second; a single-flight guard
    // keeps overlapping requests from piling up if COSMOS responds slowly,
    // exactly like PluginViewModel/ScriptViewModel's job pollers.
    if (pollInFlight_ && pollWorker_.isRunning()) return;

    const std::string target = targetName.toStdString();
    const std::string packet = packetName.toStdString();
    pollInFlight_ = true;
    pollWorker_ = QtConcurrent::run([this, target, packet] {
        auto values = runtime_.telemetryValues(target, packet);
        QMetaObject::invokeMethod(this,
            [this, list = std::move(values)]() mutable {
                pollInFlight_ = false;
                telemetryModel_->setValues(std::move(list));
                emit telemetryValuesChanged();
            },
            Qt::QueuedConnection);
    });
}

void RuntimeViewModel::setTelemetryPolling(
    const QString& targetName, const QString& packetName)
{
    pollTarget_ = targetName;
    pollPacket_ = packetName;
    if (pollTarget_.isEmpty() || pollPacket_.isEmpty()) {
        pollTimer_->stop();
        return;
    }
    refreshTelemetryValues(pollTarget_, pollPacket_);
    pollTimer_->start();
}

} // namespace OpenC3::ViewModels
