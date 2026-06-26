#include "LogViewerView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QFontDatabase>
#include <QScrollBar>
#include <QComboBox>

namespace OpenC3::UI::Views {

LogViewerView::LogViewerView(
    ViewModels::LogViewerViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void LogViewerView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("Log Viewer", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Connection hint ───────────────────────────────────────────────────────
    connectionHint_ = new QLabel(
        "Connect to a remote environment to stream logs.", this);
    connectionHint_->setObjectName("SubLabel");
    root->addWidget(connectionHint_);

    // ── Command bar ───────────────────────────────────────────────────────────
    auto* cmdBar = new QHBoxLayout;
    presetCombo_ = new QComboBox(this);
    presetCombo_->addItem("COSMOS Log",
        "tail -f /cosmos/outputs/logs/server.log");
    presetCombo_->addItem("System Journal",
        "journalctl -f -n 50");
    presetCombo_->addItem("Docker Compose Logs",
        "docker compose -f /cosmos/compose.yaml logs -f --tail=50");
    presetCombo_->addItem("Custom…", "");

    commandEdit_ = new QLineEdit(this);
    commandEdit_->setPlaceholderText("shell command to stream…");

    startStopBtn_   = new QPushButton("▶  Start", this);
    startStopBtn_->setObjectName("PrimaryButton");
    clearBtn_       = new QPushButton("🗑  Clear", this);
    autoScrollCheck_ = new QCheckBox("Auto-scroll", this);
    autoScrollCheck_->setChecked(true);

    cmdBar->addWidget(new QLabel("Preset:", this));
    cmdBar->addWidget(presetCombo_);
    cmdBar->addWidget(commandEdit_);
    cmdBar->addWidget(startStopBtn_);
    cmdBar->addWidget(clearBtn_);
    cmdBar->addWidget(autoScrollCheck_);
    root->addLayout(cmdBar);

    // ── Log area ──────────────────────────────────────────────────────────────
    logView_ = new QTextEdit(this);
    logView_->setReadOnly(true);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    logView_->setFont(mono);
    logView_->setObjectName("LogArea");
    root->addWidget(logView_);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    // Populate command edit from first preset
    commandEdit_->setText(presetCombo_->currentData().toString());
}

void LogViewerView::bindViewModel()
{
    connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                const QString val = presetCombo_->itemData(idx).toString();
                if (!val.isEmpty()) commandEdit_->setText(val);
            });

    connect(startStopBtn_, &QPushButton::clicked,
            this, &LogViewerView::onStartStopClicked);

    connect(clearBtn_, &QPushButton::clicked,
            logView_, &QTextEdit::clear);

    connect(&vm_, &ViewModels::LogViewerViewModel::connectionChanged,
            this, [this] {
                const bool on = vm_.isConnected();
                connectionHint_->setVisible(!on);
                startStopBtn_->setEnabled(on || vm_.isStreaming());
            });

    connect(&vm_, &ViewModels::LogViewerViewModel::streamingChanged,
            this, [this] {
                const bool streaming = vm_.isStreaming();
                startStopBtn_->setText(streaming ? "■  Stop" : "▶  Start");
                presetCombo_->setEnabled(!streaming);
                commandEdit_->setEnabled(!streaming);
            });

    connect(&vm_, &ViewModels::LogViewerViewModel::statusMessageChanged,
            this, [this] {
                statusLabel_->setText(vm_.statusMessage());
            });

    connect(&vm_, &ViewModels::LogViewerViewModel::logLineReceived,
            this, &LogViewerView::appendLine);

    connect(&vm_, &ViewModels::LogViewerViewModel::streamEnded,
            this, [this] {
                appendLine("--- stream ended ---");
            });

    // Initial state
    const bool on = vm_.isConnected();
    connectionHint_->setVisible(!on);
    startStopBtn_->setEnabled(on);
}

void LogViewerView::onStartStopClicked()
{
    if (vm_.isStreaming()) {
        vm_.stopStream();
    } else {
        const QString cmd = commandEdit_->text().trimmed();
        if (!cmd.isEmpty()) vm_.startStream(cmd);
    }
}

void LogViewerView::appendLine(const QString& line)
{
    logView_->append(line);
    if (autoScrollCheck_->isChecked()) {
        auto* sb = logView_->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}

} // namespace OpenC3::UI::Views
