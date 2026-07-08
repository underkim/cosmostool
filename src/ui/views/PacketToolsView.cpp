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
#include <QTabWidget>

namespace OpenC3::UI::Views {

namespace {

// Colour used for inline error text (matches the simulator-error styling).
constexpr const char* kErrorStyle = "color:#f48771;"; // soft red

// Lightweight, immediate check of a hex payload for UI feedback. The
// authoritative parse still lives in PacketSimulator::parseHexPayload; this just
// mirrors its rules so the user gets feedback while typing, before sending.
struct HexCheck {
    enum class State { Empty, Invalid, Valid };
    State   state{State::Empty};
    QString error;
};

HexCheck checkHexPayload(const QString& raw)
{
    QString s = raw;
    s.remove(' ');
    s.remove('\t');
    s.remove('\n');
    s.remove('\r');
    s.remove(QStringLiteral("0x"), Qt::CaseInsensitive);

    if (s.isEmpty())
        return {HexCheck::State::Empty, {}};

    const auto isHexDigit = [](QChar c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    for (const QChar c : s) {
        if (!isHexDigit(c)) {
            return {HexCheck::State::Invalid,
                    PacketToolsView::tr("Hex can only contain 0-9 and A-F (e.g. 01 02 A0 FF).")};
        }
    }
    if ((s.size() % 2) != 0) {
        return {HexCheck::State::Invalid,
                PacketToolsView::tr("Hex needs an even number of digits — use byte pairs like 01 02 A0 FF.")};
    }
    return {HexCheck::State::Valid, {}};
}

} // namespace

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
    auto* title = new QLabel(tr("Packet Tools"), this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Connection hint ───────────────────────────────────────────────────────
    connectionHint_ = new QLabel(
        tr("Connect to a remote environment to browse packet logs."), this);
    connectionHint_->setObjectName("SubLabel");
    root->addWidget(connectionHint_);

    // ── Tabs: keep log analysis and the simulator on separate pages ────────────
    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildLogsTab(),      tr("Logs"));
    tabs->addTab(buildSimulatorTab(), tr("Simulator"));
    tabs->setCurrentIndex(0); // default to Logs
    root->addWidget(tabs, 1);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);
}

QWidget* PacketToolsView::buildLogsTab()
{
    auto* tab    = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(12);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    refreshBtn_  = new QPushButton(tr("↻  Refresh Logs"), tab);
    progressBar_ = new QProgressBar(tab);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    progressBar_->setFixedWidth(120);
    toolbar->addWidget(refreshBtn_);
    toolbar->addStretch();
    toolbar->addWidget(progressBar_);
    layout->addLayout(toolbar);

    // ── Splitter: log list / content + analysis ───────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, tab);

    // Short title - QGroupBox titles don't wrap or elide, so the full path
    // clipped mid-word when the panel was narrower than the title text. Full
    // path is still discoverable via the tooltip.
    auto* leftGroup  = new QGroupBox(tr("Packet Logs"), splitter);
    leftGroup->setToolTip("/cosmos/outputs/logs/tlm/");
    auto* leftLayout = new QVBoxLayout(leftGroup);
    logList_ = new QListWidget(leftGroup);
    leftLayout->addWidget(logList_);

    auto* rightPane   = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* contentGroup  = new QGroupBox(tr("File Content"), rightPane);
    auto* contentLayout = new QVBoxLayout(contentGroup);
    contentView_ = new QTextEdit(contentGroup);
    contentView_->setReadOnly(true);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    contentView_->setFont(mono);
    contentView_->setObjectName("LogArea");
    contentLayout->addWidget(contentView_);

    auto* filterRow = new QHBoxLayout;
    filterEdit_  = new QLineEdit(rightPane);
    filterEdit_->setPlaceholderText(tr("Filter / keyword…"));
    analyzeBtn_  = new QPushButton(tr("Analyze"), rightPane);
    filterRow->addWidget(new QLabel(tr("Filter:"), rightPane));
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
    layout->addWidget(splitter, 1);

    return tab;
}

QWidget* PacketToolsView::buildSimulatorTab()
{
    auto* tab    = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(12);

    // ── Server: choose transport, bind address/port, start/stop ────────────────
    auto* serverGroup = new QGroupBox(tr("Server"), tab);
    auto* serverRow   = new QHBoxLayout(serverGroup);
    simulatorMode_ = new QComboBox(serverGroup);
    simulatorMode_->addItem(tr("UDP Listener"), QStringLiteral("udp"));
    simulatorMode_->addItem(tr("TCP Server"), QStringLiteral("tcp"));

    simulatorBindAddress_ = new QLineEdit(serverGroup);
    simulatorBindAddress_->setPlaceholderText(tr("Bind address"));
    simulatorBindAddress_->setText("0.0.0.0");

    simulatorBindPort_ = new QSpinBox(serverGroup);
    simulatorBindPort_->setRange(1, 65535);
    simulatorBindPort_->setValue(8080);

    simulatorStartBtn_ = new QPushButton(tr("Start"), serverGroup);
    simulatorStopBtn_  = new QPushButton(tr("Stop"), serverGroup);
    simulatorStopBtn_->setEnabled(false);

    serverRow->addWidget(new QLabel(tr("Mode:"), serverGroup));
    serverRow->addWidget(simulatorMode_);
    serverRow->addWidget(new QLabel(tr("Bind:"), serverGroup));
    serverRow->addWidget(simulatorBindAddress_);
    serverRow->addWidget(simulatorBindPort_);
    serverRow->addWidget(simulatorStartBtn_);
    serverRow->addWidget(simulatorStopBtn_);
    layout->addWidget(serverGroup);

    // ── Send: payload + (UDP) destination, with quick example payloads ─────────
    auto* sendGroup  = new QGroupBox(tr("Send"), tab);
    auto* sendLayout = new QVBoxLayout(sendGroup);

    auto* sendRow = new QHBoxLayout;
    simulatorSendHostLabel_ = new QLabel(tr("Destination host:"), sendGroup);
    simulatorSendHost_ = new QLineEdit(sendGroup);
    simulatorSendHost_->setPlaceholderText(tr("Destination host"));
    simulatorSendHost_->setText("127.0.0.1");

    simulatorSendPortLabel_ = new QLabel(tr("Port:"), sendGroup);
    simulatorSendPort_ = new QSpinBox(sendGroup);
    simulatorSendPort_->setRange(1, 65535);
    simulatorSendPort_->setValue(8080);

    simulatorPayload_ = new QLineEdit(sendGroup);
    simulatorPayload_->setPlaceholderText(tr("Hex payload, e.g. 01 02 A0 FF"));

    simulatorSendBtn_ = new QPushButton(tr("Send"), sendGroup);

    sendRow->addWidget(simulatorSendHostLabel_);
    sendRow->addWidget(simulatorSendHost_);
    sendRow->addWidget(simulatorSendPortLabel_);
    sendRow->addWidget(simulatorSendPort_);
    sendRow->addWidget(new QLabel(tr("Hex:"), sendGroup));
    sendRow->addWidget(simulatorPayload_, 1);
    sendRow->addWidget(simulatorSendBtn_);
    sendLayout->addLayout(sendRow);

    // Quick example payloads — one click fills the hex field.
    auto* exampleRow = new QHBoxLayout;
    exampleRow->addWidget(new QLabel(tr("Examples:"), sendGroup));
    auto addExample = [&](const QString& hex) {
        auto* btn = new QPushButton(hex, sendGroup);
        btn->setToolTip(tr("Use this payload"));
        connect(btn, &QPushButton::clicked, this,
                [this, hex] { simulatorPayload_->setText(hex); });
        exampleRow->addWidget(btn);
    };
    addExample("01 02 A0 FF");
    addExample("CA FE");
    addExample("50 49 4E 47"); // "PING"
    exampleRow->addStretch();
    sendLayout->addLayout(exampleRow);

    simulatorSendHint_ = new QLabel(sendGroup);
    simulatorSendHint_->setObjectName("SubLabel");
    simulatorSendHint_->setWordWrap(true);
    sendLayout->addWidget(simulatorSendHint_);

    // Live readiness / connection state (and friendly send errors, in red).
    simulatorSendStatus_ = new QLabel(sendGroup);
    simulatorSendStatus_->setObjectName("SubLabel");
    simulatorSendStatus_->setWordWrap(true);
    sendLayout->addWidget(simulatorSendStatus_);

    layout->addWidget(sendGroup);

    // ── Packet table ───────────────────────────────────────────────────────────
    // Nothing ever clears this table (not on Start/Stop, not on switching
    // UDP/TCP mode), so a long-running session just accumulates rows
    // forever with no way to reset short of restarting the app - add a
    // Clear button, matching the one Docker Manager's Container Logs panel
    // already has for the same kind of accumulating log view.
    auto* packetsHeaderRow = new QHBoxLayout;
    packetsHeaderRow->addWidget(new QLabel(tr("Captured Packets"), tab));
    packetsHeaderRow->addStretch();
    auto* simulatorClearBtn = new QPushButton(tr("Clear"), tab);
    packetsHeaderRow->addWidget(simulatorClearBtn);
    layout->addLayout(packetsHeaderRow);

    simulatorPackets_ = new QTableWidget(0, 4, tab);
    simulatorPackets_->setHorizontalHeaderLabels({tr("Direction"), tr("Peer"), tr("Hex"), tr("ASCII")});
    simulatorPackets_->horizontalHeader()->setStretchLastSection(true);
    simulatorPackets_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    simulatorPackets_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    simulatorPackets_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    simulatorPackets_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    simulatorPackets_->setSelectionBehavior(QAbstractItemView::SelectRows);
    simulatorPackets_->setMinimumHeight(140);
    layout->addWidget(simulatorPackets_, 1);

    connect(simulatorClearBtn, &QPushButton::clicked,
            simulatorPackets_, [this] { simulatorPackets_->setRowCount(0); });

    updateSimulatorSendMode();  // set initial labels for the default (UDP) mode
    updateSimulatorSendState(); // set initial Send-button + readiness text
    return tab;
}

void PacketToolsView::updateSimulatorSendMode()
{
    const bool udp = (simulatorMode_->currentData().toString() == QStringLiteral("udp"));

    // UDP datagrams go to an explicit destination; TCP sends to the connected
    // client, so the host/port fields don't apply there.
    simulatorSendHostLabel_->setVisible(udp);
    simulatorSendHost_->setVisible(udp);
    simulatorSendPortLabel_->setVisible(udp);
    simulatorSendPort_->setVisible(udp);

    simulatorSendHint_->setText(udp
        ? tr("UDP: the payload is sent to the destination host/port above.")
        : tr("TCP: the payload is sent to the connected client (start the server "
          "and let a client connect first)."));

    updateSimulatorSendState(); // readiness depends on the transport too
}

void PacketToolsView::updateSimulatorSendState()
{
    const bool      udp     = (simulatorMode_->currentData().toString() == QStringLiteral("udp"));
    const bool      running = vm_.simulatorRunning();
    const HexCheck  hex     = checkHexPayload(simulatorPayload_->text());
    const bool      empty   = (hex.state == HexCheck::State::Empty);

    bool    canSend = false;
    bool    isError = false;
    QString status;

    if (hex.state == HexCheck::State::Invalid) {
        // A malformed payload can never be sent, whatever the transport — flag it
        // immediately so the user can fix it before pressing Send.
        isError = true;
        status  = hex.error;
    } else if (udp) {
        // A UDP datagram is fire-and-forget; it only needs a valid payload.
        canSend = !empty;
        status  = empty ? tr("Enter a hex payload to send.")
                        : tr("Ready to send a UDP datagram.");
    } else if (!running) {
        status = tr("Start the TCP server, then wait for a client to connect.");
    } else if (tcpClientCount_ <= 0) {
        status = tr("Waiting for a TCP client to connect…");
    } else if (empty) {
        status = tr("%1 client(s) connected. Enter a hex payload to send.")
                     .arg(tcpClientCount_);
    } else {
        canSend = true;
        status  = tr("%1 client(s) connected. Ready to send.")
                     .arg(tcpClientCount_);
    }

    simulatorSendBtn_->setEnabled(canSend);
    simulatorSendStatus_->setStyleSheet(isError ? QString::fromLatin1(kErrorStyle) : QString());
    simulatorSendStatus_->setText(status);
}

void PacketToolsView::bindViewModel()
{
    connect(refreshBtn_, &QPushButton::clicked,
            &vm_, &ViewModels::PacketToolsViewModel::refreshLogList);

    // A single click already selects the row, so preview its content right
    // away too - previously only itemDoubleClicked loaded the content, which
    // left the File Content pane silently empty after a single click with no
    // hint that a preview exists at all.
    connect(logList_, &QListWidget::itemClicked,
            this, &PacketToolsView::onLogFileSelected);
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
                for (const QString& f : files) {
                    // Real COSMOS log names ("<start>_<end>_<target>_all.bin")
                    // routinely overflow the panel width - the list only shows
                    // them via horizontal scroll, so make the full name
                    // reachable on hover too.
                    auto* item = new QListWidgetItem(f, logList_);
                    item->setToolTip(f);
                }
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

    connect(simulatorMode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { updateSimulatorSendMode(); });

    connect(simulatorPayload_, &QLineEdit::textChanged,
            this, [this] { updateSimulatorSendState(); });

    connect(&vm_, &ViewModels::PacketToolsViewModel::tcpClientCountChanged,
            this, [this](int count) {
                tcpClientCount_ = count;
                updateSimulatorSendState();
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorStateChanged,
            this, [this] {
                const bool running = vm_.simulatorRunning();
                simulatorStartBtn_->setEnabled(!running);
                simulatorStopBtn_->setEnabled(running);
                simulatorMode_->setEnabled(!running);
                simulatorBindAddress_->setEnabled(!running);
                simulatorBindPort_->setEnabled(!running);
                tcpClientCount_ = vm_.tcpClientCount();
                updateSimulatorSendState();
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorError,
            this, [this](const QString& reason) {
                // Friendly inline message (e.g. send failure, no client)…
                simulatorSendStatus_->setStyleSheet(QString::fromLatin1(kErrorStyle));
                simulatorSendStatus_->setText(reason);
                // …plus a row in the packet table for the running history.
                const int row = simulatorPackets_->rowCount();
                simulatorPackets_->insertRow(row);
                simulatorPackets_->setItem(row, 0, new QTableWidgetItem(tr("ERROR")));
                simulatorPackets_->setItem(row, 1, new QTableWidgetItem(tr("Simulator")));
                simulatorPackets_->setItem(row, 2, new QTableWidgetItem(reason));
                simulatorPackets_->setItem(row, 3, new QTableWidgetItem(QString()));
                simulatorPackets_->scrollToBottom();
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorPacketReceived,
            this, [this](const QString& transport, const QString& peer,
                         const QString& hexPayload, const QString& asciiPreview) {
                const int row = simulatorPackets_->rowCount();
                simulatorPackets_->insertRow(row);
                simulatorPackets_->setItem(row, 0, new QTableWidgetItem(tr("RX ") + transport));
                simulatorPackets_->setItem(row, 1, new QTableWidgetItem(peer));
                simulatorPackets_->setItem(row, 2, new QTableWidgetItem(hexPayload));
                simulatorPackets_->setItem(row, 3, new QTableWidgetItem(asciiPreview));
                simulatorPackets_->scrollToBottom();
            });

    connect(&vm_, &ViewModels::PacketToolsViewModel::simulatorPacketSent,
            this, [this](const QString& transport, const QString& peer,
                         const QString& hexPayload, const QString& asciiPreview) {
                const int row = simulatorPackets_->rowCount();
                simulatorPackets_->insertRow(row);
                simulatorPackets_->setItem(row, 0, new QTableWidgetItem(tr("TX ") + transport));
                simulatorPackets_->setItem(row, 1, new QTableWidgetItem(peer));
                simulatorPackets_->setItem(row, 2, new QTableWidgetItem(hexPayload));
                simulatorPackets_->setItem(row, 3, new QTableWidgetItem(asciiPreview));
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
    if (mode == QStringLiteral("tcp")) {
        vm_.sendTcpSimulatorPacket(simulatorPayload_->text().trimmed());
        return;
    }

    vm_.sendUdpSimulatorPacket(simulatorSendHost_->text().trimmed(),
                               static_cast<quint16>(simulatorSendPort_->value()),
                               simulatorPayload_->text().trimmed());
}

} // namespace OpenC3::UI::Views
