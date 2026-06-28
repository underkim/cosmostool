#include "PacketToolsView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QFontDatabase>
#include <QListWidgetItem>

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

} // namespace OpenC3::UI::Views
