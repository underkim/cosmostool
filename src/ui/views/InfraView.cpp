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

    auto* title = new QLabel(tr("Infrastructure Manager"), this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    statusLabel_ = new QLabel("", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(buildEnvTab(),            tr(".env Editor"));
    tabs_->addTab(buildComposeTab(),        tr("compose.yaml"));
    tabs_->addTab(buildVolumeOverrideTab(), tr("Volume Overrides"));
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
    envLoadBtn_  = new QPushButton(tr("Load"), page);
    envSaveBtn_  = new QPushButton(tr("Save"), page);
    envSaveBtn_->setObjectName("PrimaryButton");
    pathBar->addWidget(new QLabel(tr("Remote path:"), page));
    pathBar->addWidget(envPathEdit_, 1);
    pathBar->addWidget(envLoadBtn_);
    pathBar->addWidget(envSaveBtn_);
    layout->addLayout(pathBar);

    // ── Sync buttons ──────────────────────────────────────────────────────────
    auto* syncBar = new QHBoxLayout;
    envSyncToRawBtn_   = new QPushButton(tr("Table -> Raw"), page);
    envSyncToTableBtn_ = new QPushButton(tr("Raw -> Table"), page);
    envSyncToRawBtn_->setToolTip(tr("Apply table edits to the raw text"));
    envSyncToTableBtn_->setToolTip(tr("Apply raw-text edits to the table"));
    syncBar->addWidget(envSyncToRawBtn_);
    syncBar->addWidget(envSyncToTableBtn_);
    syncBar->addStretch();
    // Password/secret-looking values (e.g. OPENC3_API_PASSWORD) were shown in
    // plain text in the table by default - visible to anyone glancing at the
    // screen or a screenshot. Mask them by default; this checkbox reveals
    // the real, editable values on demand.
    showSecretsCheck_ = new QCheckBox(tr("Show secrets"), page);
    syncBar->addWidget(showSecretsCheck_);
    layout->addLayout(syncBar);

    // ── Splitter: table (left) | raw text (right) ─────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, page);

    envTable_ = new QTableWidget(0, 3, page);
    envTable_->setHorizontalHeaderLabels({tr("Key"), tr("Value"), tr("Comment")});
    envTable_->horizontalHeader()->setStretchLastSection(true);
    envTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    envTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    // Interactive columns start at Qt's small default width - env values (paths,
    // hostnames, etc.) are often longer than that and were clipping to a few
    // characters. Give Value a reasonable, still user-resizable, default.
    envTable_->setColumnWidth(1, 220);
    envTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    envTable_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    QFont tf = envTable_->font(); tf.setFamily("Consolas"); envTable_->setFont(tf);

    envRawEdit_ = new QPlainTextEdit(page);
    QFont rf; rf.setFamily("Consolas"); rf.setPointSize(10);
    envRawEdit_->setFont(rf);
    envRawEdit_->setPlaceholderText(tr("# Edit as KEY=VALUE lines"));

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
    composeLoadBtn_ = new QPushButton(tr("Load"), page);
    composeSaveBtn_ = new QPushButton(tr("Save"), page);
    composeSaveBtn_->setObjectName("PrimaryButton");
    pathBar->addWidget(new QLabel(tr("Remote path:"), page));
    pathBar->addWidget(composePath_, 1);
    pathBar->addWidget(composeLoadBtn_);
    pathBar->addWidget(composeSaveBtn_);
    layout->addLayout(pathBar);

    // ── Editor ────────────────────────────────────────────────────────────────
    composeEdit_ = new QPlainTextEdit(page);
    QFont f; f.setFamily("Consolas"); f.setPointSize(10);
    composeEdit_->setFont(f);
    composeEdit_->setPlaceholderText(tr(
        "# compose.yaml is not loaded\n"
        "# Enter the remote file path above and press Load"));
    layout->addWidget(composeEdit_, 1);

    // ── Hint ─────────────────────────────────────────────────────────────────
    auto* hint = new QLabel(tr(
        "<small style='color:#858585'>"
        "Changes take effect after restarting the services "
        "(<code>docker compose up -d</code>)</small>"), page);
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
    auto* desc = new QLabel(tr(
        "<small style='color:#858585'>"
        "Extract a file from a Docker image, edit it, save it on the host, and "
        "generate the matching <code>volumes:</code> entry for compose.yaml. "
        "This overrides container files without rebuilding the image."
        "</small>"), page);
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    layout->addWidget(desc);

    // ── Step 1: pick a container and extract a file ──────────────────────────
    auto* step1Group  = new QGroupBox(tr("Step 1 - Extract a container file"), page);
    auto* step1Layout = new QVBoxLayout(step1Group);

    auto* containerBar = new QHBoxLayout;
    containerCombo_     = new QComboBox(step1Group);
    containerCombo_->setMinimumWidth(220);
    containerCombo_->setPlaceholderText(tr("Select a container…"));
    containerRefreshBtn_ = new QPushButton("↻", step1Group);
    containerRefreshBtn_->setFixedWidth(32);
    containerRefreshBtn_->setToolTip(tr("Refresh the list of running containers"));
    containerBar->addWidget(new QLabel(tr("Container:"), step1Group));
    containerBar->addWidget(containerCombo_, 1);
    containerBar->addWidget(containerRefreshBtn_);
    step1Layout->addLayout(containerBar);

    auto* fileBar = new QHBoxLayout;
    containerFilePathEdit_ = new QLineEdit(step1Group);
    containerFilePathEdit_->setPlaceholderText("/cosmos/config/system.txt");
    containerFilePathEdit_->setFont(QFont("Consolas", 10));
    extractBtn_ = new QPushButton(tr("Extract"), step1Group);
    extractBtn_->setToolTip(tr("Fetches the file via docker exec <container> cat <file>"));
    fileBar->addWidget(new QLabel(tr("File path:"), step1Group));
    fileBar->addWidget(containerFilePathEdit_, 1);
    fileBar->addWidget(extractBtn_);
    step1Layout->addLayout(fileBar);

    containerFileEdit_ = new QPlainTextEdit(step1Group);
    QFont ef; ef.setFamily("Consolas"); ef.setPointSize(9);
    containerFileEdit_->setFont(ef);
    containerFileEdit_->setPlaceholderText(tr(
        "Press Extract to show the container file's content here.\n"
        "Edit it as needed, then save it in Step 2 below."));
    containerFileEdit_->setMinimumHeight(180);
    step1Layout->addWidget(containerFileEdit_, 1);

    layout->addWidget(step1Group, 1);

    // ── Step 2: save on the host and generate the volume entry ───────────────
    auto* step2Group  = new QGroupBox(tr("Step 2 - Save and generate the volume entry"), page);
    auto* step2Layout = new QVBoxLayout(step2Group);

    auto* hostBar = new QHBoxLayout;
    hostSavePathEdit_ = new QLineEdit(step2Group);
    hostSavePathEdit_->setPlaceholderText("/cosmos/overrides/…");
    hostSavePathEdit_->setFont(QFont("Consolas", 10));
    hostSavePathEdit_->setToolTip(tr(
        "WSL/Linux path where the file will be saved.\n"
        "This path is used in the compose.yaml volumes entry."));
    applyOverrideBtn_ = new QPushButton(tr("Save && Generate Entry"), step2Group);
    applyOverrideBtn_->setObjectName("PrimaryButton");
    applyOverrideBtn_->setToolTip(tr("Extract a file in Step 1 first."));
    hostBar->addWidget(new QLabel(tr("Host save path:"), step2Group));
    hostBar->addWidget(hostSavePathEdit_, 1);
    hostBar->addWidget(applyOverrideBtn_);
    step2Layout->addLayout(hostBar);

    // ── Generated volume entry ────────────────────────────────────────────────
    auto* volumeHintLabel = new QLabel(
        tr("<code>volumes:</code> entry to add to compose.yaml:"), step2Group);
    volumeHintLabel->setTextFormat(Qt::RichText);
    step2Layout->addWidget(volumeHintLabel);

    volumeEntryEdit_ = new QPlainTextEdit(step2Group);
    volumeEntryEdit_->setFont(QFont("Consolas", 9));
    volumeEntryEdit_->setReadOnly(true);
    volumeEntryEdit_->setMaximumHeight(90);
    volumeEntryEdit_->setPlaceholderText(tr("The entry appears here after saving…"));
    step2Layout->addWidget(volumeEntryEdit_);

    auto* actionBar = new QHBoxLayout;
    copyVolumeEntryBtn_  = new QPushButton(tr("Copy to Clipboard"), step2Group);
    insertToComposeBtn_  = new QPushButton(tr("Insert into compose Editor"), step2Group);
    copyVolumeEntryBtn_->setToolTip(tr("Save & Generate Entry above first."));
    insertToComposeBtn_->setToolTip(tr("Save & Generate Entry above first."));
    actionBar->addWidget(copyVolumeEntryBtn_);
    actionBar->addWidget(insertToComposeBtn_);
    actionBar->addStretch();
    step2Layout->addLayout(actionBar);

    layout->addWidget(step2Group);

    // ── Hint ─────────────────────────────────────────────────────────────────
    auto* restartHint = new QLabel(tr(
        "<small style='color:#858585'>"
        "After adding the volume entry, restart the container with "
        "<code>docker compose up -d &lt;service-name&gt;</code> to apply the override."
        "</small>"), page);
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
    connect(showSecretsCheck_, &QCheckBox::toggled, this, [this] {
        suppressTableSignal_ = true;
        applySecretMasking();
        suppressTableSignal_ = false;
    });

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
                    QMessageBox::warning(this, tr("Save Failed"),
                        tr("Could not save the file:\n%1").arg(path));
            });

    connect(&vm_, &ViewModels::InfraViewModel::containersLoaded,
            this, [this](const QStringList& names) {
                const QString prev = containerCombo_->currentText();
                containerCombo_->clear();
                containerCombo_->addItems(names);
                // Restore the previous selection if it's still present, otherwise
                // default to the first container - addItems() alone leaves
                // currentIndex() at -1 (placeholderText showing "Select a
                // container…" forever) even once entries exist, which silently
                // no-ops Extract until the user manually opens the dropdown.
                const int idx = containerCombo_->findText(prev);
                if (idx >= 0) containerCombo_->setCurrentIndex(idx);
                else if (!names.isEmpty()) containerCombo_->setCurrentIndex(0);
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
                    QMessageBox::warning(this, tr("Save Failed"),
                        tr("Could not save the host file:\n%1").arg(hostPath));
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
        QMessageBox::information(this, tr("Select Container"),
            tr("Select a container, or press the refresh button to load the list."));
        return;
    }
    if (filePath.isEmpty()) {
        QMessageBox::information(this, tr("File Path"),
            tr("Enter the path of the file inside the container to extract."));
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
        QMessageBox::information(this, tr("Save Path"),
            tr("Enter the host (WSL) path where the file will be saved.\n"
            "Example: /cosmos/overrides/cosmos/config/system.txt"));
        return;
    }
    if (content.isEmpty()) {
        QMessageBox::information(this, tr("No Content"),
            tr("Extract a file from the container first, or enter content to save."));
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
        auto* valueItem = new QTableWidgetItem(value);
        valueItem->setData(Qt::UserRole, value);
        envTable_->setItem(row, 1, valueItem);
        envTable_->setItem(row, 2, new QTableWidgetItem(comment));
    }

    applySecretMasking();
    suppressTableSignal_ = false;
}

namespace {
bool isSecretEnvKey(const QString& key)
{
    static const QStringList kNeedles = {"PASSWORD", "SECRET", "TOKEN", "KEY"};
    for (const QString& needle : kNeedles)
        if (key.contains(needle, Qt::CaseInsensitive))
            return true;
    return false;
}
} // namespace

void InfraView::applySecretMasking()
{
    const bool reveal = showSecretsCheck_->isChecked();
    for (int row = 0; row < envTable_->rowCount(); ++row) {
        const auto* keyItem = envTable_->item(row, 0);
        auto*       valItem = envTable_->item(row, 1);
        if (!keyItem || !valItem || !isSecretEnvKey(keyItem->text()))
            continue;

        if (reveal) {
            valItem->setText(valItem->data(Qt::UserRole).toString());
            valItem->setFlags(valItem->flags() | Qt::ItemIsEditable);
        } else {
            // Snapshot the current (possibly just-edited) real value before
            // masking, so re-checking "Show secrets" later restores it
            // rather than whatever was loaded at the start of the session.
            valItem->setData(Qt::UserRole, valItem->text());
            valItem->setText(QStringLiteral("••••••••"));
            valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
        }
    }
}

QString InfraView::collectTableToEnv() const
{
    QString result;
    for (int row = 0; row < envTable_->rowCount(); ++row) {
        const auto* keyItem = envTable_->item(row, 0);
        const auto* valItem = envTable_->item(row, 1);
        const auto* cmtItem = envTable_->item(row, 2);

        if (!keyItem) continue;
        const QString key = keyItem->text();
        // A masked secret row displays "••••••••" in valItem->text() - the
        // real value lives in UserRole while masked (see applySecretMasking).
        const bool    masked = valItem && !(valItem->flags() & Qt::ItemIsEditable)
                                && isSecretEnvKey(key);
        const QString value   = valItem ? (masked ? valItem->data(Qt::UserRole).toString()
                                                    : valItem->text())
                                          : QString{};
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
