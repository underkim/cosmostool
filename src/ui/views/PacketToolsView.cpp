#include "PacketToolsView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QFontDatabase>
#include <QListWidgetItem>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QTableWidgetItem>

namespace OpenC3::UI::Views {

PacketToolsView::PacketToolsView(
    ViewModels::PacketToolsViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void PacketToolsView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("Packet Tools", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Connection hint ───────────────────────────────────────────────────────
    connectionHint_ = new QLabel(
        "Connect to a remote environment to browse packet logs.", this);
    connectionHint_->setObjectName("SubLabel");
    root->addWidget(connectionHint_);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    refreshBtn_  = new QPushButton("↻  Refresh Logs", this);
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    progressBar_->setFixedWidth(120);
    toolbar->addWidget(refreshBtn_);
    toolbar->addStretch();
    toolbar->addWidget(progressBar_);
    root->addLayout(toolbar);

    // ── Splitter: log list / content + analysis ───────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftGroup  = new QGroupBox("Packet Logs (/cosmos/outputs/logs/tlm/)", splitter);
    auto* leftLayout = new QVBoxLayout(leftGroup);
    logList_ = new QListWidget(leftGroup);
    leftLayout->addWidget(logList_);

    auto* rightPane   = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* contentGroup  = new QGroupBox("File Content", rightPane);
    auto* contentLayout = new QVBoxLayout(contentGroup);
    contentView_ = new QTextEdit(contentGroup);
    contentView_->setReadOnly(true);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    contentView_->setFont(mono);
    contentView_->setObjectName("LogArea");
    contentLayout->addWidget(contentView_);

    auto* filterRow = new QHBoxLayout;
    filterEdit_  = new QLineEdit(rightPane);
    filterEdit_->setPlaceholderText("Filter / keyword…");
    analyzeBtn_  = new QPushButton("Analyze", rightPane);
    filterRow->addWidget(new QLabel("Filter:", rightPane));
    filterRow->addWidget(filterEdit_);
    filterRow->addWidget(analyzeBtn_);

    analysisLabel_ = new QLabel(rightPane);
    analysisLabel_->setObjectName("SubLabel");
    analysisLabel_->setWordWrap(true);

    rightLayout->addWidget(contentGroup);
    rightLayout->addLayout(filterRow);
    rightLayout->addWidget(analysisLabel_);

    splitter->addWidget(leftGroup);
    splitter->addWidget(rightPane);
    splitter->setChildrenCollapsible(false); // list and content pane stay visible
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    leftGroup->setMinimumWidth(200);
    rightPane->setMinimumWidth(320);
    splitter->setSizes({280, 900});
    root->addWidget(splitter);

    // ── Simulator controls ───────────────────────────────────────────────────
    auto* simulatorGroup = new QGroupBox("Peer Equipment Simulator", this);
    auto* simulatorLayout = new QVBoxLayout(simulatorGroup);

    auto* simulatorControlRow = new QHBoxLayout;
    simulatorMode_ = new QComboBox(simulatorGroup);
    simulatorMode_->addItem("UDP Listener", QStringLiteral("udp"));
    simulatorMode_->addItem("TCP Server", QStringLiteral("tcp"));

    simulatorBindAddress_ = new QLineEdit(simulatorGroup);
    simulatorBindAddress_->setPlaceholderText("Bind address");
    simulatorBindAddress_->setText("0.0.0.0");

    simulatorBindPort_ = new QSpinBox(simulatorGroup);
    simulatorBindPort_->setRange(1, 65535);
    simulatorBindPort_->setValue(8080);

    simulatorStartBtn_ = new QPushButton("Start", simulatorGroup);
    simulatorStopBtn_ = new QPushButton("Stop", simulatorGroup);
    simulatorStopBtn_->setEnabled(false);

    simulatorControlRow->addWidget(new QLabel("Mode:", simulatorGroup));
    simulatorControlRow->addWidget(simulatorMode_);
    simulatorControlRow->addWidget(new QLabel("Bind:", simulatorGroup));
    simulatorControlRow->addWidget(simulatorBindAddress_);
    simulatorControlRow->addWidget(simulatorBindPort_);
    simulatorControlRow->addWidget(simulatorStartBtn_);
    simulatorControlRow->addWidget(simulatorStopBtn_);

    auto* simulatorSendRow = new QHBoxLayout;
    simulatorSendHost_ = new QLineEdit(simulatorGroup);
    simulatorSendHost_->setPlaceholderText("Destination host");
    simulatorSendHost_->setText("127.0.0.1");

    simulatorSendPort_ = new QSpinBox(simulatorGroup);
    simulatorSendPort_->setRange(1, 65535);
    simulatorSendPort_->setValue(8080);

    simulatorPayload_ = new QLineEdit(simulatorGroup);
    simulatorPayload_->setPlaceholderText("Hex payload, e.g. 01 02 A0 FF");

    simulatorSendBtn_ = new QPushButton("Send", simulatorGroup);

    simulatorSendRow->addWidget(new QLabel("Send UDP host:", simulatorGroup));
    simulatorSendRow->addWidget(simulatorSendHost_);
    simulatorSendRow->addWidget(new QLabel("Port:", simulatorGroup));
    simulatorSendRow->addWidget(simulatorSendPort_);
    simulatorSendRow->addWidget(new QLabel("Hex:", simulatorGroup));
    simulatorSendRow->addWidget(simulatorPayload_, 1);
    simulatorSendRow->addWidget(simulatorSendBtn_);

    simulatorPackets_ = new QTableWidget(0, 4, simulatorGroup);
    simulatorPackets_->setHorizontalHeaderLabels({"Direction", "Peer", "Hex", "ASCII"});
    simulatorPackets_->horizontalHeader()->setStretchLastSection(true);
    simulatorPackets_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    simulatorPackets_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    simulatorPackets_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    simulatorPackets_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    simulatorPackets_->setSelectionBehavior(QAbstractItemView::SelectRows);
    simulatorPackets_->setMinimumHeight(140);

    simulatorLayout->addLayout(simulatorControlRow);
    simulatorLayout->addLayout(simulatorSendRow);
    simulatorLayout->addWidget(simulatorPackets_);
    root->addWidget(simulatorGroup);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);
}

void PacketToolsView::bindViewModel()
{
    connect(refreshBtn_, &QPushButton::clicked,
            &vm_, &ViewModels::PacketToolsViewModel::refreshLogList);

    connect(logList_, &QListWidget::itemDoubleClicked,
            this, &PacketToolsView::onLogFileSelected);

    connect(analyzeBtn_, &QPushButton::clicked,
            this, &PacketToolsView::onAnalyzeClicked);

    connect(&vm_, &ViewModels::PacketToolsViewModel::connectionChanged,
            this, [this] {
                const bool on = vm_.isConnected();
                connectionHint_->setVisible(!on);
                refreshBtn_->setEnabled(on);
                analyzeBtn_->setEnabled(on);
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::busyChanged,
            this, [this] {
                progressBar_->setVisible(vm_.isBusy());
                refreshBtn_->setEnabled(vm_.isConnected() && !vm_.isBusy());
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::statusMessageChanged,
            this, [this] { statusLabel_->setText(vm_.statusMessage()); });

    connect(&vm_, &ViewModels::PacketToolsViewModel::logListReady,
            this, [this](const QStringList& files) {
                logList_->clear();
                for (const QString& f : files)
                    logList_->addItem(f);
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::logContentReady,
            this, [this](const QString& /*path*/, const QString& content) {
                contentView_->setPlainText(content);
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::analysisReady,
            this, [this](const QString& summary) {
                analysisLabel_->setText(summary);
            });

    connect(simulatorStartBtn_, &QPushButton::clicked,
            this, &PacketToolsView::onSimulatorStartClicked);

    connect(simulatorStopBtn_, &QPushButton::clicked,
            this, &PacketToolsView::onSimulatorStopClicked);

    connect(simulatorSendBtn_, &QPushButton::clicked,
            this, &PacketToolsView::onSimulatorSendClicked);

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorStateChanged,
            this, [this] {
                const bool running = vm_.simulatorRunning();
                simulatorStartBtn_->setEnabled(!running);
                simulatorStopBtn_->setEnabled(running);
                simulatorMode_->setEnabled(!running);
                simulatorBindAddress_->setEnabled(!running);
                simulatorBindPort_->setEnabled(!running);
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorError,
            this, [this](const QString& reason) {
                const int row = simulatorPackets_->rowCount();
                simulatorPackets_->insertRow(row);
                simulatorPackets_->setItem(row, 0, new QTableWidgetItem("ERROR"));
                simulatorPackets_->setItem(row, 1, new QTableWidgetItem("Simulator"));
                simulatorPackets_->setItem(row, 2, new QTableWidgetItem(reason));
                simulatorPackets_->setItem(row, 3, new QTableWidgetItem(QString()));
                simulatorPackets_->scrollToBottom();
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorPacketReceived,
            this, [this](const QString& transport, const QString& peer,
                         const QString& hexPayload, const QString& asciiPreview) {
                const int row = simulatorPackets_->rowCount();
                simulatorPackets_->insertRow(row);
                simulatorPackets_->setItem(row, 0, new QTableWidgetItem("RX " + transport));
                simulatorPackets_->setItem(row, 1, new QTableWidgetItem(peer));
                simulatorPackets_->setItem(row, 2, new QTableWidgetItem(hexPayload));
                simulatorPackets_->setItem(row, 3, new QTableWidgetItem(asciiPreview));
                simulatorPackets_->scrollToBottom();
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorPacketSent,
            this, [this](const QString& transport, const QString& peer,
                         const QString& hexPayload) {
                const int row = simulatorPackets_->rowCount();
                simulatorPackets_->insertRow(row);
                simulatorPackets_->setItem(row, 0, new QTableWidgetItem("TX " + transport));
                simulatorPackets_->setItem(row, 1, new QTableWidgetItem(peer));
                simulatorPackets_->setItem(row, 2, new QTableWidgetItem(hexPayload));
                simulatorPackets_->setItem(row, 3, new QTableWidgetItem(QString()));
                simulatorPackets_->scrollToBottom();
            });

    // Initial state
    const bool on = vm_.isConnected();
    connectionHint_->setVisible(!on);
    refreshBtn_->setEnabled(on);
    analyzeBtn_->setEnabled(on);
}

void PacketToolsView::onLogFileSelected(QListWidgetItem* item)
{
    if (!item) return;
    const QString path = QString("/cosmos/outputs/logs/tlm/") + item->text();
    vm_.openLogFile(path);
}

void PacketToolsView::onAnalyzeClicked()
{
    const auto* item = logList_->currentItem();
    if (!item) return;
    const QString path = QString("/cosmos/outputs/logs/tlm/") + item->text();
    vm_.analyzePackets(path, filterEdit_->text().trimmed());
}

void PacketToolsView::onSimulatorStartClicked()
{
    const QString mode = simulatorMode_->currentData().toString();
    if (mode == QStringLiteral("udp")) {
        vm_.startUdpSimulator(simulatorBindAddress_->text().trimmed(),
                              static_cast<quint16>(simulatorBindPort_->value()));
    } else {
        vm_.startTcpSimulator(simulatorBindAddress_->text().trimmed(),
                              static_cast<quint16>(simulatorBindPort_->value()));
    }
}

void PacketToolsView::onSimulatorStopClicked()
{
    vm_.stopSimulator();
}

void PacketToolsView::onSimulatorSendClicked()
{
    const QString mode = simulatorMode_->currentData().toString();
    if (mode == QStringLiteral("tcp") && vm_.simulatorRunning()) {
        vm_.sendTcpSimulatorPacket(simulatorPayload_->text().trimmed());
        return;
    }

    vm_.sendUdpSimulatorPacket(simulatorSendHost_->text().trimmed(),
                               static_cast<quint16>(simulatorSendPort_->value()),
                               simulatorPayload_->text().trimmed());
}

} // namespace OpenC3::UI::Views
