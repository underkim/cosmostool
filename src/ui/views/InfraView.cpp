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
    tabs_->addTab(buildEnvTab(),            "🔐 .env 편집");
    tabs_->addTab(buildComposeTab(),        "🐙 compose.yaml");
    tabs_->addTab(buildVolumeOverrideTab(), "🐳 볼륨 오버라이드");
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
    envSyncToRawBtn_   = new QPushButton("⬇ Table → Raw", page);
    envSyncToTableBtn_ = new QPushButton("⬆ Raw → Table", page);
    envSyncToRawBtn_->setToolTip("테이블 편집 내용을 원문에 반영");
    envSyncToTableBtn_->setToolTip("원문 편집 내용을 테이블에 반영");
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
    envRawEdit_->setPlaceholderText("# KEY=VALUE 형식으로 편집");

    splitter->addWidget(envTable_);
    splitter->addWidget(envRawEdit_);
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
        "# compose.yaml 이 로드되지 않았습니다\n"
        "# 원격 파일 경로를 입력하고 Load를 눌러주세요");
    layout->addWidget(composeEdit_, 1);

    // ── Hint ─────────────────────────────────────────────────────────────────
    auto* hint = new QLabel(
        "<small style='color:#858585'>"
        "⚠️  저장 후 서비스를 재시작해야 변경사항이 적용됩니다 "
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

    // ── 설명 ─────────────────────────────────────────────────────────────────
    auto* desc = new QLabel(
        "<small style='color:#858585'>"
        "Docker 이미지 내부의 파일을 추출 → 편집 → 호스트에 저장하고, "
        "compose.yaml의 <code>volumes:</code> 항목을 생성합니다. "
        "이미지를 재빌드하지 않고 컨테이너 파일을 오버라이드할 수 있습니다."
        "</small>", page);
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    layout->addWidget(desc);

    // ── Step 1: 컨테이너 선택 + 파일 추출 ────────────────────────────────────
    auto* step1Group  = new QGroupBox("① 컨테이너 파일 추출", page);
    auto* step1Layout = new QVBoxLayout(step1Group);

    auto* containerBar = new QHBoxLayout;
    containerCombo_     = new QComboBox(step1Group);
    containerCombo_->setMinimumWidth(220);
    containerCombo_->setPlaceholderText("컨테이너 선택…");
    containerRefreshBtn_ = new QPushButton("↻", step1Group);
    containerRefreshBtn_->setFixedWidth(32);
    containerRefreshBtn_->setToolTip("실행 중인 컨테이너 목록 새로고침");
    containerBar->addWidget(new QLabel("컨테이너:", step1Group));
    containerBar->addWidget(containerCombo_, 1);
    containerBar->addWidget(containerRefreshBtn_);
    step1Layout->addLayout(containerBar);

    auto* fileBar = new QHBoxLayout;
    containerFilePathEdit_ = new QLineEdit(step1Group);
    containerFilePathEdit_->setPlaceholderText("/cosmos/config/system.txt");
    containerFilePathEdit_->setFont(QFont("Consolas", 10));
    extractBtn_ = new QPushButton("⬇ 추출", step1Group);
    extractBtn_->setToolTip("docker exec <container> cat <file>로 파일 내용을 가져옵니다");
    fileBar->addWidget(new QLabel("파일 경로:", step1Group));
    fileBar->addWidget(containerFilePathEdit_, 1);
    fileBar->addWidget(extractBtn_);
    step1Layout->addLayout(fileBar);

    containerFileEdit_ = new QPlainTextEdit(step1Group);
    QFont ef; ef.setFamily("Consolas"); ef.setPointSize(9);
    containerFileEdit_->setFont(ef);
    containerFileEdit_->setPlaceholderText(
        "추출 버튼을 누르면 컨테이너 내부 파일 내용이 여기에 표시됩니다.\n"
        "내용을 직접 편집한 후 아래 ② 단계에서 저장하세요.");
    containerFileEdit_->setMinimumHeight(180);
    step1Layout->addWidget(containerFileEdit_, 1);

    layout->addWidget(step1Group, 1);

    // ── Step 2: 호스트 저장 + 볼륨 항목 생성 ────────────────────────────────
    auto* step2Group  = new QGroupBox("② 저장 경로 설정 및 볼륨 항목 생성", page);
    auto* step2Layout = new QVBoxLayout(step2Group);

    auto* hostBar = new QHBoxLayout;
    hostSavePathEdit_ = new QLineEdit(step2Group);
    hostSavePathEdit_->setPlaceholderText("/cosmos/overrides/…");
    hostSavePathEdit_->setFont(QFont("Consolas", 10));
    hostSavePathEdit_->setToolTip(
        "파일이 저장될 WSL/Linux 경로입니다.\n"
        "이 경로가 compose.yaml의 volumes 항목에 사용됩니다.");
    applyOverrideBtn_ = new QPushButton("✔ 저장 및 항목 생성", step2Group);
    applyOverrideBtn_->setObjectName("PrimaryButton");
    hostBar->addWidget(new QLabel("호스트 저장 경로:", step2Group));
    hostBar->addWidget(hostSavePathEdit_, 1);
    hostBar->addWidget(applyOverrideBtn_);
    step2Layout->addLayout(hostBar);

    // ── 생성된 볼륨 항목 ──────────────────────────────────────────────────────
    auto* volumeHintLabel = new QLabel(
        "compose.yaml에 추가할 <code>volumes:</code> 항목:", step2Group);
    volumeHintLabel->setTextFormat(Qt::RichText);
    step2Layout->addWidget(volumeHintLabel);

    volumeEntryEdit_ = new QPlainTextEdit(step2Group);
    volumeEntryEdit_->setFont(QFont("Consolas", 9));
    volumeEntryEdit_->setReadOnly(true);
    volumeEntryEdit_->setMaximumHeight(90);
    volumeEntryEdit_->setPlaceholderText("저장 후 여기에 항목이 생성됩니다…");
    step2Layout->addWidget(volumeEntryEdit_);

    auto* actionBar = new QHBoxLayout;
    copyVolumeEntryBtn_  = new QPushButton("📋 클립보드 복사", step2Group);
    insertToComposeBtn_  = new QPushButton("→ compose 편집기에 삽입", step2Group);
    actionBar->addWidget(copyVolumeEntryBtn_);
    actionBar->addWidget(insertToComposeBtn_);
    actionBar->addStretch();
    step2Layout->addLayout(actionBar);

    layout->addWidget(step2Group);

    // ── 안내 ─────────────────────────────────────────────────────────────────
    auto* restartHint = new QLabel(
        "<small style='color:#858585'>"
        "볼륨 항목 추가 후: <code>docker compose up -d &lt;service-name&gt;</code> "
        "으로 컨테이너를 재시작하면 오버라이드가 적용됩니다."
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
                    QMessageBox::warning(this, "저장 실패",
                        "파일 저장에 실패했습니다:\n" + path);
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
                    QMessageBox::warning(this, "저장 실패",
                        "호스트 파일 저장 실패:\n" + hostPath);
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
        QMessageBox::information(this, "컨테이너 선택",
            "컨테이너를 선택하거나 새로고침 버튼으로 목록을 불러오세요.");
        return;
    }
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "파일 경로",
            "추출할 파일의 컨테이너 내부 경로를 입력하세요.");
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
        QMessageBox::information(this, "저장 경로",
            "파일이 저장될 호스트(WSL) 경로를 입력하세요.\n"
            "예: /cosmos/overrides/cosmos/config/system.txt");
        return;
    }
    if (content.isEmpty()) {
        QMessageBox::information(this, "내용 없음",
            "먼저 컨테이너에서 파일을 추출하거나 내용을 입력하세요.");
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

        const int eq = line.indexOf('=');
        if (eq < 0) continue;

        const QString key     = line.left(eq).trimmed();
        QString       value   = line.mid(eq + 1).trimmed();
        QString       comment = pendingComment;
        pendingComment.clear();

        // Inline comment: VALUE  # comment
        const int hashIdx = value.indexOf(" #");
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
