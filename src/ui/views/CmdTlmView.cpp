#include "CmdTlmView.h"
#include "ui/dialogs/CmdTlmFieldDialog.h"
#include "ui/widgets/CmdTlmHighlighter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QFont>
#include <QFontDatabase>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QTextBlock>
#include <QTextCursor>

namespace OpenC3::UI::Views {

// ?? Templates inserted at cursor ??????????????????????????????????????????????

static const QString kCmdTemplate =
    "COMMAND TARGET_NAME COMMAND_NAME BIG_ENDIAN \"Command description\"\n"
    "  APPEND_ID_PARAMETER CMD_ID 16 UINT 1 1 1 \"Command ID\"\n"
    "  APPEND_PARAMETER PARAM1 32 FLOAT 0 100 0.0 \"Parameter description\"\n";

static const QString kTlmTemplate =
    "TELEMETRY TARGET_NAME PACKET_NAME BIG_ENDIAN \"Telemetry description\"\n"
    "  APPEND_ITEM SYNC_WORD 32 UINT \"Sync pattern\"\n"
    "  APPEND_ITEM ITEM1 32 FLOAT \"Item description\"\n";

static const QString kParamTemplate =
    "  APPEND_PARAMETER PARAM_NAME 32 FLOAT 0.0 100.0 0.0 \"Parameter description\"\n";

// ?? Constructor ???????????????????????????????????????????????????????????????

static QString syntaxGuideText()
{
    return QStringLiteral(
        "CMD/TLM Quick Reference\n\n"
        "Block headers\n"
        "  COMMAND <TARGET> <NAME> <ENDIANNESS> \"description\"\n"
        "  TELEMETRY <TARGET> <NAME> <ENDIANNESS> \"description\"\n"
        "  ENDIANNESS: BIG_ENDIAN or LITTLE_ENDIAN\n\n"
        "Command parameters\n"
        "  APPEND_PARAMETER <NAME> <BIT_SIZE> <TYPE> <MIN> <MAX> <DEFAULT> \"description\"\n"
        "  PARAMETER is used when you need an explicit bit offset before the name.\n"
        "  APPEND_ID_PARAMETER marks a command identifier field.\n\n"
        "Telemetry items\n"
        "  APPEND_ITEM <NAME> <BIT_SIZE> <TYPE> \"description\"\n"
        "  ITEM is used when you need an explicit bit offset before the name.\n"
        "  APPEND_ID_ITEM marks a telemetry identifier field.\n\n"
        "Common data types\n"
        "  UINT, INT, FLOAT\n"
        "  UINT8, UINT16, UINT32, UINT64\n"
        "  INT8, INT16, INT32, INT64\n"
        "  FLOAT32, FLOAT64\n"
        "  STRING, BLOCK, DERIVED\n\n"
        "Common child keywords\n"
        "  STATE <LABEL> <VALUE> \"description\"\n"
        "  UNITS <FULL_NAME> <ABBREVIATION>\n"
        "  FORMAT_STRING \"0x%04X\"\n"
        "  REQUIRED, HIDDEN, HAZARDOUS\n"
        "  LIMITS, LIMITS_RESPONSE\n"
        "  READ_CONVERSION, WRITE_CONVERSION\n\n"
        "Examples\n"
        "  COMMAND FSW RESET BIG_ENDIAN \"Reset command\"\n"
        "    APPEND_PARAMETER CMD_ID 8 UINT 0 255 1 \"Command ID\"\n\n"
        "  TELEMETRY FSW HK BIG_ENDIAN \"Housekeeping\"\n"
        "    APPEND_ITEM TEMP 16 INT \"Temperature\"\n"
        "      UNITS Celsius C\n");
}

CmdTlmView::CmdTlmView(ViewModels::CmdTlmViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

// ?? setupUi ???????????????????????????????????????????????????????????????????

void CmdTlmView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(8);

    // ?? Title ?????????????????????????????????????????????????????????????????
    auto* title = new QLabel("CMD / TLM Editor", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf);
    title->setObjectName("PageTitle");
    root->addWidget(title);

    // ?? Connection hint ???????????????????????????????????????????????????????
    connectionHint_ = new QLabel(
        "Connect to a remote environment to browse and edit definition files.", this);
    connectionHint_->setObjectName("SubLabel");
    root->addWidget(connectionHint_);

    // ── Toolbar row 1 — path ──────────────────────────────────────
    auto* pathBar = new QHBoxLayout;
    pathEdit_  = new QLineEdit(this);
    browseBtn_ = new QPushButton("Browse", this);
    pathBar->addWidget(new QLabel("Path:", this));
    pathBar->addWidget(pathEdit_, 1);
    pathBar->addWidget(browseBtn_);
    root->addLayout(pathBar);

    // ── Toolbar row 2 — editor actions ────────────────────────────
    auto* actionBar = new QHBoxLayout;
    insertCmdBtn_   = new QPushButton("+ COMMAND",   this);
    insertTlmBtn_   = new QPushButton("+ TELEMETRY", this);
    insertParamBtn_ = new QPushButton("+ PARAMETER", this);
    addFieldBtn_    = new QPushButton("Add Field...", this);
    validateBtn_    = new QPushButton("Validate", this);
    saveBtn_        = new QPushButton("Save", this);
    saveBtn_->setObjectName("PrimaryButton");
    saveBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    insertParamBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);
    validateBtn_->setEnabled(false);
    actionBar->addWidget(insertCmdBtn_);
    actionBar->addWidget(insertTlmBtn_);
    actionBar->addWidget(insertParamBtn_);
    actionBar->addWidget(addFieldBtn_);
    actionBar->addSpacing(16);
    actionBar->addWidget(validateBtn_);
    actionBar->addStretch();
    actionBar->addWidget(saveBtn_);
    root->addLayout(actionBar);

    // ?? Main splitter (vertical) ??????????????????????????????????????????????
    auto* vSplit = new QSplitter(Qt::Vertical, this);

    // ?? Top: file browser | editor ????????????????????????????????????????????
    auto* hSplit = new QSplitter(Qt::Horizontal, vSplit);

    // File list (left)
    auto* fileGroup  = new QGroupBox("Files", hSplit);
    auto* fileLayout = new QVBoxLayout(fileGroup);
    fileList_ = new QListWidget(fileGroup);
    fileLayout->addWidget(fileList_);

    // Editor (right)
    auto* editorGroup  = new QGroupBox("Editor", hSplit);
    auto* editorLayout = new QVBoxLayout(editorGroup);
    fileLabel_ = new QLabel("(no file open)", editorGroup);
    fileLabel_->setObjectName("SubLabel");
    editor_ = new QPlainTextEdit(editorGroup);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editor_->setFont(mono);
    editor_->setObjectName("LogArea");
    highlighter_ = new Widgets::CmdTlmHighlighter(editor_->document());
    editorLayout->addWidget(fileLabel_);
    editorLayout->addWidget(editor_);

    auto* guideGroup = new QGroupBox("Reference", hSplit);
    auto* guideLayout = new QVBoxLayout(guideGroup);
    syntaxGuide_ = new QTextEdit(guideGroup);
    syntaxGuide_->setObjectName("LogArea");
    syntaxGuide_->setReadOnly(true);
    syntaxGuide_->setLineWrapMode(QTextEdit::WidgetWidth);
    syntaxGuide_->setPlainText(syntaxGuideText());
    guideLayout->addWidget(syntaxGuide_);

    hSplit->addWidget(fileGroup);
    hSplit->addWidget(editorGroup);
    hSplit->addWidget(guideGroup);
    hSplit->setSizes({220, 760, 320});
    vSplit->addWidget(hSplit);

    // ?? Middle: structure tree ????????????????????????????????????????????????
    auto* structGroup  = new QGroupBox("Structure", vSplit);
    auto* structLayout = new QVBoxLayout(structGroup);
    structureTree_ = new QTreeWidget(structGroup);
    structureTree_->setHeaderLabels({"Name", "Type", "Bits", "Description"});
    structureTree_->header()->setStretchLastSection(true);
    structureTree_->setRootIsDecorated(true);
    structureTree_->setAlternatingRowColors(true);
    structLayout->addWidget(structureTree_);
    vSplit->addWidget(structGroup);

    // ?? Bottom: diagnostics ???????????????????????????????????????????????????
    auto* diagGroup  = new QGroupBox("Diagnostics", vSplit);
    auto* diagLayout = new QVBoxLayout(diagGroup);
    diagSummary_ = new QLabel("", diagGroup);
    diagSummary_->setObjectName("SubLabel");
    diagnosticList_ = new QListWidget(diagGroup);
    diagnosticList_->setMaximumHeight(120);
    diagLayout->addWidget(diagSummary_);
    diagLayout->addWidget(diagnosticList_);
    vSplit->addWidget(diagGroup);

    vSplit->setSizes({420, 180, 120});
    root->addWidget(vSplit, 1);

    // ?? Status bar ????????????????????????????????????????????????????????????
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    // Initial path
    pathEdit_->setText("/cosmos/targets");
}

// ?? bindViewModel ?????????????????????????????????????????????????????????????

void CmdTlmView::bindViewModel()
{
    connect(browseBtn_,     &QPushButton::clicked, this, &CmdTlmView::onBrowseClicked);
    connect(saveBtn_,       &QPushButton::clicked, this, &CmdTlmView::onSaveClicked);
    connect(validateBtn_,   &QPushButton::clicked, this, &CmdTlmView::onValidateClicked);
    connect(insertCmdBtn_,  &QPushButton::clicked, this, &CmdTlmView::onInsertCmd);
    connect(insertTlmBtn_,  &QPushButton::clicked, this, &CmdTlmView::onInsertTlm);
    connect(insertParamBtn_,&QPushButton::clicked, this, &CmdTlmView::onInsertParam);
    connect(addFieldBtn_,   &QPushButton::clicked, this, &CmdTlmView::onAddField);

    connect(fileList_, &QListWidget::itemDoubleClicked,
            this, &CmdTlmView::onFileItemDoubleClicked);

    connect(structureTree_, &QTreeWidget::itemClicked,
            this, &CmdTlmView::onStructureItemClicked);

    connect(diagnosticList_, &QListWidget::itemClicked,
            this, &CmdTlmView::onDiagnosticItemClicked);

    connect(&vm_, &ViewModels::CmdTlmViewModel::connectionChanged,
            this, [this] {
                const bool on = vm_.isConnected();
                connectionHint_->setVisible(!on);
                browseBtn_->setEnabled(on);
                pathEdit_->setEnabled(on);
                if (on)
                    pathEdit_->setText(vm_.defaultCmdTlmPath());
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::busyChanged,
            this, [this] {
                const bool on  = vm_.isConnected();
                const bool busy = vm_.isBusy();
                browseBtn_->setEnabled(on && !busy);
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::statusMessageChanged,
            this, [this] {
                statusLabel_->setText(vm_.statusMessage());
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::directoryListed,
            this, [this](const QStringList& entries, const QString& path) {
                currentDir_ = path;
                fileList_->clear();
                for (const QString& e : entries)
                    fileList_->addItem(e);
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileOpened,
            this, [this](const QString& path, const QString& content) {
                currentFile_ = path;
                fileLabel_->setText(path);
                editor_->setPlainText(content);
                const bool isTxt = path.endsWith(".txt", Qt::CaseInsensitive);
                saveBtn_->setEnabled(true);
                validateBtn_->setEnabled(isTxt);
                insertCmdBtn_->setEnabled(isTxt);
                insertTlmBtn_->setEnabled(isTxt);
                insertParamBtn_->setEnabled(isTxt);
                addFieldBtn_->setEnabled(isTxt);
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileSaved,
            this, [this](const QString& /*path*/, bool ok) {
                if (!ok)
                    QMessageBox::warning(this, "Save Failed",
                        "Could not write the file to the remote host.");
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileParsed,
            this, &CmdTlmView::onFileParsed);

    // Initial state
    const bool on = vm_.isConnected();
    connectionHint_->setVisible(!on);
    browseBtn_->setEnabled(on);
    pathEdit_->setEnabled(on);
    if (on) pathEdit_->setText(vm_.defaultCmdTlmPath());
}

// ?? Slots ?????????????????????????????????????????????????????????????????????

void CmdTlmView::openDirectory(const QString& remotePath)
{
    const QString path = remotePath.trimmed();
    if (path.isEmpty()) return;
    currentDir_ = path;
    pathEdit_->setText(path);
    vm_.listDirectory(path);
}

void CmdTlmView::openFile(const QString& remotePath)
{
    const QString path = remotePath.trimmed();
    if (path.isEmpty()) return;

    const int slash = path.lastIndexOf('/');
    if (slash > 0) {
        currentDir_ = path.left(slash);
        pathEdit_->setText(currentDir_);
        vm_.listDirectory(currentDir_);
    }

    vm_.openFile(path);
}

void CmdTlmView::onBrowseClicked()
{
    vm_.listDirectory(pathEdit_->text().trimmed());
}

void CmdTlmView::onFileItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString name = item->text();
    const QString path = (currentDir_.isEmpty() || currentDir_.endsWith('/'))
                         ? currentDir_ + name
                         : currentDir_ + '/' + name;

    // Heuristic: no extension or trailing slash → directory
    if (!name.contains('.') || name.endsWith('/')) {
        pathEdit_->setText(path.endsWith('/') ? path.chopped(1) : path);
        vm_.listDirectory(pathEdit_->text());
    } else {
        vm_.openFile(path);
    }
}

void CmdTlmView::onSaveClicked()
{
    if (currentFile_.isEmpty()) return;
    vm_.saveFile(currentFile_, editor_->toPlainText());
}

void CmdTlmView::onValidateClicked()
{
    vm_.parseContent(editor_->toPlainText(), currentFile_);
}

void CmdTlmView::onInsertCmd()   { insertTextAtCursor(kCmdTemplate); }
void CmdTlmView::onInsertTlm()   { insertTextAtCursor(kTlmTemplate); }
void CmdTlmView::onInsertParam() { insertTextAtCursor(kParamTemplate); }

void CmdTlmView::onAddField()
{
    Dialogs::CmdTlmFieldDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        insertTextAtCursor(dialog.generatedLine());
}

void CmdTlmView::onStructureItemClicked(QTreeWidgetItem* item, int /*col*/)
{
    if (!item) return;
    const int line = item->data(0, Qt::UserRole).toInt();
    if (line > 0) scrollEditorToLine(line);
}

void CmdTlmView::onDiagnosticItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    const int line = item->data(Qt::UserRole).toInt();
    if (line > 0) scrollEditorToLine(line);
}

void CmdTlmView::onFileParsed(const ViewModels::CmdTlmParseResult& result,
                               const QString& /*filePath*/)
{
    populateStructureTree(result);
    populateDiagnostics(result);
}

// ?? Helpers ???????????????????????????????????????????????????????????????????

void CmdTlmView::populateStructureTree(const ViewModels::CmdTlmParseResult& result)
{
    structureTree_->clear();

    for (const auto& block : result.blocks) {
        const bool isCmd = (block.kind == ViewModels::CmdTlmBlock::Kind::Command);
        const QString label = QString("[%1] %2::%3")
            .arg(isCmd ? "CMD" : "TLM")
            .arg(block.targetName)
            .arg(block.name);

        auto* blockItem = new QTreeWidgetItem(structureTree_);
        blockItem->setText(0, label);
        blockItem->setText(3, block.description);
        blockItem->setData(0, Qt::UserRole, block.lineNumber);
        // Colour: commands blue, telemetry teal
        blockItem->setForeground(0, isCmd ? QColor("#569CD6") : QColor("#4EC994"));

        for (const auto& item : block.items) {
            auto* child = new QTreeWidgetItem(blockItem);
            child->setText(0, item.name);
            child->setText(1, item.dataType);
            child->setText(2, QString::number(item.bitSize));
            child->setText(3, item.description);
            child->setData(0, Qt::UserRole, item.lineNumber);
            if (item.isId)
                child->setForeground(0, QColor("#CE9178"));
        }
        blockItem->setExpanded(true);
    }

    structureTree_->resizeColumnToContents(0);
    structureTree_->resizeColumnToContents(1);
    structureTree_->resizeColumnToContents(2);
}

void CmdTlmView::populateDiagnostics(const ViewModels::CmdTlmParseResult& result)
{
    diagnosticList_->clear();

    const int e = result.errorCount();
    const int w = result.warningCount();

    if (result.diagnostics.isEmpty()) {
        diagSummary_->setText("✅ No issues found.");
    } else {
        diagSummary_->setText(QString("● %1 error(s)   ▲ %2 warning(s)").arg(e).arg(w));
    }

    for (const auto& d : result.diagnostics) {
        const bool isError = (d.severity == ViewModels::CmdTlmDiagnostic::Severity::Error);
        const QString text = QString("%1 Line %2: %3")
            .arg(isError ? "●" : "▲")
            .arg(d.line)
            .arg(d.message);

        auto* li = new QListWidgetItem(text, diagnosticList_);
        li->setData(Qt::UserRole, d.line);
        li->setForeground(isError ? QColor("#F44747") : QColor("#CCA700"));
    }
}

void CmdTlmView::scrollEditorToLine(int line)
{
    if (line < 1) return;
    const QTextBlock block = editor_->document()->findBlockByLineNumber(line - 1);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    editor_->setTextCursor(cursor);
    editor_->centerCursor();
    editor_->setFocus();
}

void CmdTlmView::insertTextAtCursor(const QString& text)
{
    QTextCursor cursor = editor_->textCursor();
    // Move to end of current line before inserting
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.insertText("\n" + text);
    editor_->setTextCursor(cursor);
    editor_->setFocus();
}

} // namespace OpenC3::UI::Views
