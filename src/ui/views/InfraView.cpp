#include "InfraView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>

namespace OpenC3::UI::Views {

InfraView::InfraView(ViewModels::InfraViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

// ── UI Construction ───────────────────────────────────────────────────────────

void InfraView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    auto* title = new QLabel("Infrastructure Manager", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    statusLabel_ = new QLabel("", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(buildEnvTab(),            ".env Editor");
    tabs_->addTab(buildComposeTab(),        "compose.yaml");
    tabs_->addTab(buildVolumeOverrideTab(), "Volume Overrides");
    root->addWidget(tabs_);
}

QWidget* InfraView::buildEnvTab()
{
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ── Path bar ──────────────────────────────────────────────────────────────
    auto* pathBar = new QHBoxLayout;
    envPathEdit_ = new QLineEdit(vm_.defaultEnvPath(), page);
    envPathEdit_->setPlaceholderText("/cosmos/.env");
    envLoadBtn_  = new QPushButton("Load", page);
    envSaveBtn_  = new QPushButton("Save", page);
    envSaveBtn_->setObjectName("PrimaryButton");
    pathBar->addWidget(new QLabel("Remote path:", page));
    pathBar->addWidget(envPathEdit_, 1);
    pathBar->addWidget(envLoadBtn_);
    pathBar->addWidget(envSaveBtn_);
    layout->addLayout(pathBar);

    // ── Sync buttons ──────────────────────────────────────────────────────────
    auto* syncBar = new QHBoxLayout;
    envSyncToRawBtn_   = new QPushButton("Table -> Raw", page);
    envSyncToTableBtn_ = new QPushButton("Raw -> Table", page);
    envSyncToRawBtn_->setToolTip("Apply table edits to the raw text");
    envSyncToTableBtn_->setToolTip("Apply raw-text edits to the table");
    syncBar->addWidget(envSyncToRawBtn_);
    syncBar->addWidget(envSyncToTableBtn_);
    syncBar->addStretch();
    layout->addLayout(syncBar);

    // ── Splitter: table (left) | raw text (right) ─────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, page);

    envTable_ = new QTableWidget(0, 3, page);
    envTable_->setHorizontalHeaderLabels({"Key", "Value", "Comment"});
    envTable_->horizontalHeader()->setStretchLastSection(true);
    envTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    envTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    envTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    envTable_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    QFont tf = envTable_->font(); tf.setFamily("Consolas"); envTable_->setFont(tf);

    envRawEdit_ = new QPlainTextEdit(page);
    QFont rf; rf.setFamily("Consolas"); rf.setPointSize(10);
    envRawEdit_->setFont(rf);
    envRawEdit_->setPlaceholderText("# Edit as KEY=VALUE lines");

    splitter->addWidget(envTable_);
    splitter->addWidget(envRawEdit_);
    splitter->setChildrenCollapsible(false); // both panes stay usable on drag
    envTable_->setMinimumWidth(240);
    envRawEdit_->setMinimumWidth(240);
    splitter->setSizes({500, 500});
    layout->addWidget(splitter, 1);

    return page;
}

QWidget* InfraView::buildComposeTab()
{
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ── Path bar ──────────────────────────────────────────────────────────────
    auto* pathBar = new QHBoxLayout;
    composePath_    = new QLineEdit(vm_.defaultComposePath(), page);
    composeLoadBtn_ = new QPushButton("Load", page);
    composeSaveBtn_ = new QPushButton("Save", page);
    composeSaveBtn_->setObjectName("PrimaryButton");
    pathBar->addWidget(new QLabel("Remote path:", page));
    pathBar->addWidget(composePath_, 1);
    pathBar->addWidget(composeLoadBtn_);
    pathBar->addWidget(composeSaveBtn_);
    layout->addLayout(pathBar);

    // ── Editor ────────────────────────────────────────────────────────────────
    composeEdit_ = new QPlainTextEdit(page);
    QFont f; f.setFamily("Consolas"); f.setPointSize(10);
    composeEdit_->setFont(f);
    composeEdit_->setPlaceholderText(
        "# compose.yaml is not loaded\n"
        "# Enter the remote file path above and press Load");
    layout->addWidget(composeEdit_, 1);

    // ── Hint ─────────────────────────────────────────────────────────────────
    auto* hint = new QLabel(
        "<small style='color:#858585'>"
        "Changes take effect after restarting the services "
        "(<code>docker compose up -d</code>)</small>", page);
    hint->setTextFormat(Qt::RichText);
    layout->addWidget(hint);

    return page;
}

QWidget* InfraView::buildVolumeOverrideTab()
{
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ── Description ──────────────────────────────────────────────────────────
    auto* desc = new QLabel(
        "<small style='color:#858585'>"
        "Extract a file from a Docker image, edit it, save it on the host, and "
        "generate the matching <code>volumes:</code> entry for compose.yaml. "
        "This overrides container files without rebuilding the image."
        "</small>", page);
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    layout->addWidget(desc);

    // ── Step 1: pick a container and extract a file ──────────────────────────
    auto* step1Group  = new QGroupBox("Step 1 - Extract a container file", page);
    auto* step1Layout = new QVBoxLayout(step1Group);

    auto* containerBar = new QHBoxLayout;
    containerCombo_     = new QComboBox(step1Group);
    containerCombo_->setMinimumWidth(220);
    containerCombo_->setPlaceholderText("Select a container…");
    containerRefreshBtn_ = new QPushButton("↻", step1Group);
    containerRefreshBtn_->setFixedWidth(32);
    containerRefreshBtn_->setToolTip("Refresh the list of running containers");
    containerBar->addWidget(new QLabel("Container:", step1Group));
    containerBar->addWidget(containerCombo_, 1);
    containerBar->addWidget(containerRefreshBtn_);
    step1Layout->addLayout(containerBar);

    auto* fileBar = new QHBoxLayout;
    containerFilePathEdit_ = new QLineEdit(step1Group);
    containerFilePathEdit_->setPlaceholderText("/cosmos/config/system.txt");
    containerFilePathEdit_->setFont(QFont("Consolas", 10));
    extractBtn_ = new QPushButton("Extract", step1Group);
    extractBtn_->setToolTip("Fetches the file via docker exec <container> cat <file>");
    fileBar->addWidget(new QLabel("File path:", step1Group));
    fileBar->addWidget(containerFilePathEdit_, 1);
    fileBar->addWidget(extractBtn_);
    step1Layout->addLayout(fileBar);

    containerFileEdit_ = new QPlainTextEdit(step1Group);
    QFont ef; ef.setFamily("Consolas"); ef.setPointSize(9);
    containerFileEdit_->setFont(ef);
    containerFileEdit_->setPlaceholderText(
        "Press Extract to show the container file's content here.\n"
        "Edit it as needed, then save it in Step 2 below.");
    containerFileEdit_->setMinimumHeight(180);
    step1Layout->addWidget(containerFileEdit_, 1);

    layout->addWidget(step1Group, 1);

    // ── Step 2: save on the host and generate the volume entry ───────────────
    auto* step2Group  = new QGroupBox("Step 2 - Save and generate the volume entry", page);
    auto* step2Layout = new QVBoxLayout(step2Group);

    auto* hostBar = new QHBoxLayout;
    hostSavePathEdit_ = new QLineEdit(step2Group);
    hostSavePathEdit_->setPlaceholderText("/cosmos/overrides/…");
    hostSavePathEdit_->setFont(QFont("Consolas", 10));
    hostSavePathEdit_->setToolTip(
        "WSL/Linux path where the file will be saved.\n"
        "This path is used in the compose.yaml volumes entry.");
    applyOverrideBtn_ = new QPushButton("Save && Generate Entry", step2Group);
    applyOverrideBtn_->setObjectName("PrimaryButton");
    applyOverrideBtn_->setToolTip("Extract a file in Step 1 first.");
    hostBar->addWidget(new QLabel("Host save path:", step2Group));
    hostBar->addWidget(hostSavePathEdit_, 1);
    hostBar->addWidget(applyOverrideBtn_);
    step2Layout->addLayout(hostBar);

    // ── Generated volume entry ────────────────────────────────────────────────
    auto* volumeHintLabel = new QLabel(
        "<code>volumes:</code> entry to add to compose.yaml:", step2Group);
    volumeHintLabel->setTextFormat(Qt::RichText);
    step2Layout->addWidget(volumeHintLabel);

    volumeEntryEdit_ = new QPlainTextEdit(step2Group);
    volumeEntryEdit_->setFont(QFont("Consolas", 9));
    volumeEntryEdit_->setReadOnly(true);
    volumeEntryEdit_->setMaximumHeight(90);
    volumeEntryEdit_->setPlaceholderText("The entry appears here after saving…");
    step2Layout->addWidget(volumeEntryEdit_);

    auto* actionBar = new QHBoxLayout;
    copyVolumeEntryBtn_  = new QPushButton("Copy to Clipboard", step2Group);
    insertToComposeBtn_  = new QPushButton("Insert into compose Editor", step2Group);
    copyVolumeEntryBtn_->setToolTip("Save & Generate Entry above first.");
    insertToComposeBtn_->setToolTip("Save & Generate Entry above first.");
    actionBar->addWidget(copyVolumeEntryBtn_);
    actionBar->addWidget(insertToComposeBtn_);
    actionBar->addStretch();
    step2Layout->addLayout(actionBar);

    layout->addWidget(step2Group);

    // ── Hint ─────────────────────────────────────────────────────────────────
    auto* restartHint = new QLabel(
        "<small style='color:#858585'>"
        "After adding the volume entry, restart the container with "
        "<code>docker compose up -d &lt;service-name&gt;</code> to apply the override."
        "</small>", page);
    restartHint->setTextFormat(Qt::RichText);
    restartHint->setWordWrap(true);
    layout->addWidget(restartHint);

    return page;
}

// ── Signals / ViewModel binding ───────────────────────────────────────────────

void InfraView::bindViewModel()
{
    // ENV tab
    connect(envLoadBtn_,  &QPushButton::clicked, this, &InfraView::onEnvLoad);
    connect(envSaveBtn_,  &QPushButton::clicked, this, &InfraView::onEnvSave);
    connect(envSyncToRawBtn_,   &QPushButton::clicked,
            this, &InfraView::syncTableToRaw);
    connect(envSyncToTableBtn_, &QPushButton::clicked,
            this, &InfraView::syncRawToTable);
    connect(envTable_, &QTableWidget::cellChanged,
            this, &InfraView::onTableCellChanged);

    // Compose tab
    connect(composeLoadBtn_, &QPushButton::clicked, this, &InfraView::onComposeLoad);
    connect(composeSaveBtn_, &QPushButton::clicked, this, &InfraView::onComposeSave);

    // Volume Override tab
    connect(containerRefreshBtn_, &QPushButton::clicked,
            &vm_, &ViewModels::InfraViewModel::loadContainers);
    connect(extractBtn_,          &QPushButton::clicked,
            this, &InfraView::onExtractFile);
    connect(applyOverrideBtn_,    &QPushButton::clicked,
            this, &InfraView::onApplyOverride);
    connect(containerFilePathEdit_, &QLineEdit::textChanged,
            this, &InfraView::onContainerFilePathChanged);

    connect(copyVolumeEntryBtn_, &QPushButton::clicked, this, [this] {
        const QString text = volumeEntryEdit_->toPlainText().trimmed();
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
    });

    connect(insertToComposeBtn_, &QPushButton::clicked, this, [this] {
        const QString text = volumeEntryEdit_->toPlainText().trimmed();
        if (text.isEmpty()) return;
        // Switch to compose tab and append the volume entry
        tabs_->setCurrentIndex(1);
        composeEdit_->appendPlainText("\n" + text);
    });

    // ── ViewModel → View ──────────────────────────────────────────────────────
    connect(&vm_, &ViewModels::InfraViewModel::statusMessageChanged,
            this, [this] { statusLabel_->setText(vm_.statusMessage()); });

    // Refresh path defaults when connection is established (cosmos root may change)
    connect(&vm_, &ViewModels::InfraViewModel::connectionChanged,
            this, [this] {
                if (vm_.isConnected()) {
                    envPathEdit_->setText(vm_.defaultEnvPath());
                    composePath_->setText(vm_.defaultComposePath());
                    // Trigger container list refresh
                    vm_.loadContainers();
                }
            });

    connect(&vm_, &ViewModels::InfraViewModel::envLoaded,
            this, [this](const QString& path, const QString& content) {
                envPathEdit_->setText(path);
                envRawEdit_->setPlainText(content);
                loadEnvIntoTable(content);
            });

    connect(&vm_, &ViewModels::InfraViewModel::composeLoaded,
            this, [this](const QString& path, const QString& content) {
                composePath_->setText(path);
                composeEdit_->setPlainText(content);
            });

    connect(&vm_, &ViewModels::InfraViewModel::fileSaved,
            this, [this](const QString& path, bool ok) {
                if (!ok)
                    QMessageBox::warning(this, "Save Failed",
                        "Could not save the file:\n" + path);
            });

    connect(&vm_, &ViewModels::InfraViewModel::containersLoaded,
            this, [this](const QStringList& names) {
                const QString prev = containerCombo_->currentText();
                containerCombo_->clear();
                containerCombo_->addItems(names);
                // Restore previous selection if still present
                const int idx = containerCombo_->findText(prev);
                if (idx >= 0) containerCombo_->setCurrentIndex(idx);
            });

    connect(&vm_, &ViewModels::InfraViewModel::containerFileExtracted,
            this, [this](const QString& /*path*/, const QString& content) {
                containerFileEdit_->setPlainText(content);
                applyOverrideBtn_->setEnabled(true);
            });

    connect(&vm_, &ViewModels::InfraViewModel::volumeEntryReady,
            this, [this](const QString& entry) {
                volumeEntryEdit_->setPlainText(entry);
                copyVolumeEntryBtn_->setEnabled(true);
                insertToComposeBtn_->setEnabled(true);
            });

    connect(&vm_, &ViewModels::InfraViewModel::overrideApplied,
            this, [this](bool ok, const QString& hostPath) {
                if (!ok)
                    QMessageBox::warning(this, "Save Failed",
                        "Could not save the host file:\n" + hostPath);
            });

    // Initial state
    applyOverrideBtn_->setEnabled(false);
    copyVolumeEntryBtn_->setEnabled(false);
    insertToComposeBtn_->setEnabled(false);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void InfraView::onEnvLoad()
{
    vm_.loadEnvFile(envPathEdit_->text().trimmed());
}

void InfraView::onEnvSave()
{
    syncTableToRaw();
    vm_.saveEnvFile(envPathEdit_->text().trimmed(), envRawEdit_->toPlainText());
}

void InfraView::onComposeLoad()
{
    vm_.loadComposeFile(composePath_->text().trimmed());
}

void InfraView::onComposeSave()
{
    vm_.saveComposeFile(composePath_->text().trimmed(), composeEdit_->toPlainText());
}

void InfraView::onExtractFile()
{
    const QString container = containerCombo_->currentText().trimmed();
    const QString filePath  = containerFilePathEdit_->text().trimmed();

    if (container.isEmpty()) {
        QMessageBox::information(this, "Select Container",
            "Select a container, or press the refresh button to load the list.");
        return;
    }
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "File Path",
            "Enter the path of the file inside the container to extract.");
        return;
    }

    containerFileEdit_->clear();
    applyOverrideBtn_->setEnabled(false);
    vm_.extractContainerFile(container, filePath);
}

void InfraView::onApplyOverride()
{
    const QString container = containerCombo_->currentText().trimmed();
    const QString ctrPath   = containerFilePathEdit_->text().trimmed();
    const QString hostPath  = hostSavePathEdit_->text().trimmed();
    const QString content   = containerFileEdit_->toPlainText();

    if (hostPath.isEmpty()) {
        QMessageBox::information(this, "Save Path",
            "Enter the host (WSL) path where the file will be saved.\n"
            "Example: /cosmos/overrides/cosmos/config/system.txt");
        return;
    }
    if (content.isEmpty()) {
        QMessageBox::information(this, "No Content",
            "Extract a file from the container first, or enter content to save.");
        return;
    }

    volumeEntryEdit_->clear();
    copyVolumeEntryBtn_->setEnabled(false);
    insertToComposeBtn_->setEnabled(false);
    vm_.applyVolumeOverride(container, ctrPath, hostPath, content);
}

void InfraView::onTableCellChanged(int /*row*/, int /*col*/)
{
    if (!suppressTableSignal_) {
        syncTableToRaw();
    }
}

void InfraView::onContainerFilePathChanged(const QString& path)
{
    // Auto-fill host save path: <cosmosRoot>/overrides + filePath
    if (path.isEmpty() || !hostSavePathEdit_->text().isEmpty()) return;
    const QString stripped = path.startsWith('/') ? path.mid(1) : path;
    hostSavePathEdit_->setText(vm_.cosmosRootPath() + "/overrides/" + stripped);
}

// ── ENV table helpers ─────────────────────────────────────────────────────────

void InfraView::loadEnvIntoTable(const QString& content)
{
    suppressTableSignal_ = true;
    envTable_->setRowCount(0);

    QString pendingComment;
    for (const QString& rawLine : content.split('\n')) {
        const QString line = rawLine.trimmed();

        if (line.isEmpty()) {
            pendingComment.clear();
            continue;
        }

        if (line.startsWith('#')) {
            pendingComment = line.mid(1).trimmed();
            continue;
        }

        const qsizetype eq = line.indexOf('=');
        if (eq < 0) continue;

        const QString key     = line.left(eq).trimmed();
        QString       value   = line.mid(eq + 1).trimmed();
        QString       comment = pendingComment;
        pendingComment.clear();

        // Inline comment: VALUE  # comment
        const qsizetype hashIdx = value.indexOf(" #");
        if (hashIdx >= 0) {
            comment = value.mid(hashIdx + 2).trimmed();
            value   = value.left(hashIdx).trimmed();
        }

        const int row = envTable_->rowCount();
        envTable_->insertRow(row);
        envTable_->setItem(row, 0, new QTableWidgetItem(key));
        envTable_->setItem(row, 1, new QTableWidgetItem(value));
        envTable_->setItem(row, 2, new QTableWidgetItem(comment));
    }

    suppressTableSignal_ = false;
}

QString InfraView::collectTableToEnv() const
{
    QString result;
    for (int row = 0; row < envTable_->rowCount(); ++row) {
        const auto* keyItem = envTable_->item(row, 0);
        const auto* valItem = envTable_->item(row, 1);
        const auto* cmtItem = envTable_->item(row, 2);

        if (!keyItem) continue;
        const QString key     = keyItem->text();
        const QString value   = valItem ? valItem->text()  : QString{};
        const QString comment = cmtItem ? cmtItem->text()  : QString{};

        if (!comment.isEmpty())
            result += "# " + comment + "\n";
        result += key + "=" + value + "\n";
    }
    return result;
}

void InfraView::syncTableToRaw()
{
    envRawEdit_->setPlainText(collectTableToEnv());
}

void InfraView::syncRawToTable()
{
    loadEnvIntoTable(envRawEdit_->toPlainText());
}

} // namespace OpenC3::UI::Views
