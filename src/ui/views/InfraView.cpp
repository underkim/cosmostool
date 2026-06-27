#include "InfraView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFont>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QScrollBar>
#include <QDir>
#include <QFile>
#include <QDateTime>

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
    tabs_->addTab(buildEnvTab(),     "🔐 .env 편집");
    tabs_->addTab(buildComposeTab(), "🐙 compose.yaml");
    tabs_->addTab(buildPatchTab(),   "🩹 Patch 생성");
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
    envPathEdit_ = new QLineEdit(ViewModels::InfraViewModel::kDefaultEnvPath, page);
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
    composePath_    = new QLineEdit(ViewModels::InfraViewModel::kDefaultComposePath, page);
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
        "# compose.yaml が読み込まれていません\n"
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

QWidget* InfraView::buildPatchTab()
{
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ── Controls ──────────────────────────────────────────────────────────────
    auto* ctrlBar = new QHBoxLayout;
    patchSourceCombo_ = new QComboBox(page);
    patchSourceCombo_->addItems({".env", "compose.yaml"});
    patchGenBtn_  = new QPushButton("🔍 Diff 생성", page);
    patchSaveBtn_ = new QPushButton("💾 .patch 저장", page);
    patchSaveBtn_->setObjectName("PrimaryButton");
    ctrlBar->addWidget(new QLabel("대상 파일:", page));
    ctrlBar->addWidget(patchSourceCombo_);
    ctrlBar->addSpacing(8);
    ctrlBar->addWidget(patchGenBtn_);
    ctrlBar->addWidget(patchSaveBtn_);
    ctrlBar->addStretch();
    layout->addLayout(ctrlBar);

    // ── Diff viewer ───────────────────────────────────────────────────────────
    patchEdit_ = new QTextEdit(page);
    QFont f; f.setFamily("Consolas"); f.setPointSize(9);
    patchEdit_->setFont(f);
    patchEdit_->setReadOnly(true);
    patchEdit_->setPlaceholderText(
        "Load 탭에서 파일을 먼저 불러온 뒤 수정하고\n"
        "'Diff 생성'을 눌러 패치를 확인하세요.\n\n"
        "생성된 패치는 unified diff 형식(.patch)으로 저장할 수 있습니다.\n"
        "COSMOS 업데이트 후 설정을 재적용할 때 사용합니다:\n"
        "  patch -p1 < my_changes.patch");
    layout->addWidget(patchEdit_, 1);

    // ── Legend ────────────────────────────────────────────────────────────────
    auto* legend = new QHBoxLayout;
    auto mkDot = [&](const QString& color, const QString& label) {
        auto* lbl = new QLabel(
            "<span style='color:" + color + "'>■</span> " + label);
        lbl->setTextFormat(Qt::RichText);
        legend->addWidget(lbl);
    };
    mkDot("#4ec94e", "추가(+)");
    mkDot("#f1444c", "삭제(-)");
    mkDot("#858585", "컨텍스트");
    legend->addStretch();
    layout->addLayout(legend);

    return page;
}

// ── Signals / ViewModel binding ───────────────────────────────────────────────

void InfraView::bindViewModel()
{
    connect(envLoadBtn_,  &QPushButton::clicked, this, &InfraView::onEnvLoad);
    connect(envSaveBtn_,  &QPushButton::clicked, this, &InfraView::onEnvSave);
    connect(envSyncToRawBtn_,   &QPushButton::clicked,
            this, &InfraView::syncTableToRaw);
    connect(envSyncToTableBtn_, &QPushButton::clicked,
            this, &InfraView::syncRawToTable);

    connect(composeLoadBtn_, &QPushButton::clicked, this, &InfraView::onComposeLoad);
    connect(composeSaveBtn_, &QPushButton::clicked, this, &InfraView::onComposeSave);

    connect(patchGenBtn_,  &QPushButton::clicked, this, &InfraView::onGeneratePatch);
    connect(patchSaveBtn_, &QPushButton::clicked, this, &InfraView::onSavePatch);

    connect(envTable_, &QTableWidget::cellChanged,
            this, &InfraView::onTableCellChanged);

    // ViewModel → View
    connect(&vm_, &ViewModels::InfraViewModel::statusMessageChanged,
            this, [this] { statusLabel_->setText(vm_.statusMessage()); });

    connect(&vm_, &ViewModels::InfraViewModel::envLoaded,
            this, [this](const QString& path, const QString& content) {
                envCurrentPath_ = path;
                envBaseline_    = content;
                envRawEdit_->setPlainText(content);
                loadEnvIntoTable(content);
            });

    connect(&vm_, &ViewModels::InfraViewModel::composeLoaded,
            this, [this](const QString& path, const QString& content) {
                composeCurrentPath_ = path;
                composeBaseline_    = content;
                composeEdit_->setPlainText(content);
            });

    connect(&vm_, &ViewModels::InfraViewModel::fileSaved,
            this, [this](const QString& path, bool ok) {
                if (!ok)
                    QMessageBox::warning(this, "저장 실패",
                        "파일 저장에 실패했습니다:\n" + path);
            });

    connect(&vm_, &ViewModels::InfraViewModel::patchReady,
            this, [this](const QString& patch) {
                patchEdit_->clear();
                if (patch == "(변경 없음)") {
                    patchEdit_->setPlainText("(변경 없음 — 원본과 동일합니다)");
                    return;
                }
                // Color-code the diff lines
                for (const QString& line : patch.split('\n')) {
                    if (line.startsWith('+') && !line.startsWith("+++")) {
                        patchEdit_->setTextColor(QColor(0x4e, 0xc9, 0x4e));
                    } else if (line.startsWith('-') && !line.startsWith("---")) {
                        patchEdit_->setTextColor(QColor(0xf1, 0x44, 0x4c));
                    } else if (line.startsWith("@@")) {
                        patchEdit_->setTextColor(QColor(0x56, 0x9c, 0xd6));
                    } else {
                        patchEdit_->setTextColor(QColor(0x85, 0x85, 0x85));
                    }
                    patchEdit_->append(line);
                }
            });
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

void InfraView::onGeneratePatch()
{
    const bool isEnv = (patchSourceCombo_->currentIndex() == 0);
    const QString original = isEnv ? envBaseline_     : composeBaseline_;
    const QString current  = isEnv ? envRawEdit_->toPlainText()
                                   : composeEdit_->toPlainText();
    const QString fname    = isEnv ? ".env" : "compose.yaml";

    if (original.isEmpty()) {
        QMessageBox::information(this, "Patch",
            "먼저 파일을 Load 해야 베이스라인이 설정됩니다.");
        return;
    }

    vm_.generatePatch(original, current, fname);
}

void InfraView::onSavePatch()
{
    const QString patch = patchEdit_->toPlainText();
    if (patch.isEmpty() || patch.startsWith("(변경 없음")) {
        QMessageBox::information(this, "저장", "저장할 패치 내용이 없습니다.");
        return;
    }

    const QString defaultName =
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_cosmos.patch";
    const QString path = QFileDialog::getSaveFileName(
        this, "패치 파일 저장", QDir::homePath() + "/" + defaultName,
        "Patch files (*.patch);;All files (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(patch.toUtf8());
        QMessageBox::information(this, "저장 완료",
            "패치 파일이 저장되었습니다:\n" + path);
    } else {
        QMessageBox::warning(this, "저장 실패", "파일을 열 수 없습니다:\n" + path);
    }
}

void InfraView::onTableCellChanged(int /*row*/, int /*col*/)
{
    if (!suppressTableSignal_) {
        // Light sync: update raw edit from table on every cell change
        // Use a deferred update to avoid re-entrancy during bulk load
    }
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
            // Accumulate comment block for the next key
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
        const auto* keyItem  = envTable_->item(row, 0);
        const auto* valItem  = envTable_->item(row, 1);
        const auto* cmtItem  = envTable_->item(row, 2);

        if (!keyItem) continue;
        const QString key     = keyItem->text();
        const QString value   = valItem  ? valItem->text()  : QString{};
        const QString comment = cmtItem  ? cmtItem->text()  : QString{};

        if (!comment.isEmpty())
            result += "# " + comment + "\n";
        result += key + "=" + value + "\n";
    }
    return result;
}

void InfraView::syncTableToRaw()
{
    const QString text = collectTableToEnv();
    envRawEdit_->setPlainText(text);
}

void InfraView::syncRawToTable()
{
    loadEnvIntoTable(envRawEdit_->toPlainText());
}

} // namespace OpenC3::UI::Views
