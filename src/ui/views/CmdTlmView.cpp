#include "CmdTlmView.h"
#include "ui/dialogs/CmdTlmFieldDialog.h"
#include "ui/widgets/CmdTlmHighlighter.h"
#include "ui/widgets/CmdTlmSnippets.h"

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
#include <QTabWidget>
#include <QShortcut>
#include <QKeySequence>

namespace OpenC3::UI::Views {

// CMD/TLM insert templates are shared with PluginView (ui/widgets/CmdTlmSnippets.h).
namespace Snippets = OpenC3::UI::Widgets::CmdTlmSnippets;

// ── Constructor ───────────────────────────────────────────────────────────────

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

// ── setupUi ───────────────────────────────────────────────────────────────────

void CmdTlmView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(8);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("CMD / TLM Editor", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf);
    title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Connection hint ───────────────────────────────────────────────────────
    connectionHint_ = new QLabel(
        "Connect to a remote environment, then click Browse to choose a file from /cosmos/targets.", this);
    connectionHint_->setObjectName("SubLabel");
    root->addWidget(connectionHint_);

    // ── Toolbar — path ────────────────────────────────────────────────────────
    auto* pathBar = new QHBoxLayout;
    pathEdit_  = new QLineEdit(this);
    browseBtn_ = new QPushButton("Browse", this);
    pathBar->addWidget(new QLabel("Path:", this));
    pathBar->addWidget(pathEdit_, 1);
    pathBar->addWidget(browseBtn_);
    root->addLayout(pathBar);

    // ── Main splitter (vertical) ──────────────────────────────────────────────
    auto* vSplit = new QSplitter(Qt::Vertical, this);

    // ── Top: file browser | editor-first workspace ────────────────────────────
    auto* hSplit = new QSplitter(Qt::Horizontal, vSplit);

    // File list (left)
    auto* fileGroup  = new QGroupBox("Files", hSplit);
    auto* fileLayout = new QVBoxLayout(fileGroup);
    fileListEmptyLabel_ = new QLabel("Connect, then browse a CMD/TLM directory to list files.", fileGroup);
    fileListEmptyLabel_->setObjectName("SubLabel");
    fileListEmptyLabel_->setWordWrap(true);
    fileList_ = new QListWidget(fileGroup);
    fileLayout->addWidget(fileListEmptyLabel_);
    fileLayout->addWidget(fileList_);

    // Editor (right)
    auto* editorGroup  = new QGroupBox("Editor", hSplit);
    auto* editorLayout = new QVBoxLayout(editorGroup);

    auto* editorHeader = new QHBoxLayout;
    fileLabel_ = new QLabel("(no file open)", editorGroup);
    fileLabel_->setObjectName("SubLabel");
    auto* referenceBtn = new QPushButton("Reference", editorGroup);
    referenceBtn->setCheckable(true);
    validateBtn_    = new QPushButton("Check", editorGroup);
    openInValidatorBtn_ = new QPushButton("Validator", editorGroup);
    saveBtn_        = new QPushButton("Save", editorGroup);
    saveBtn_->setObjectName("PrimaryButton");
    saveBtn_->setEnabled(false);
    validateBtn_->setEnabled(false);
    openInValidatorBtn_->setEnabled(false);
    openInValidatorBtn_->setToolTip(
        "Open in the Validator view for the full per-rule (offline) check.");
    // No hardcoded pixel-width floor: each button's own sizeHint already
    // reserves exactly the space its text needs for the current font/DPI,
    // so a smaller fixed minimum here would be what actually risks clipping.
    editorHeader->addWidget(fileLabel_, 1);
    editorHeader->addWidget(referenceBtn);
    editorHeader->addWidget(validateBtn_);
    editorHeader->addWidget(openInValidatorBtn_);
    editorHeader->addWidget(saveBtn_);
    editorLayout->addLayout(editorHeader);

    // Hidden backing buttons: kept only so the existing clicked-signal wiring
    // is untouched. The user-visible control is insertMenuBtn_ below, whose
    // menu actions trigger the same slots (mirrors the Check/Insert/Fields
    // menu pattern already used in PluginView).
    insertCmdBtn_   = new QPushButton("+ COMMAND",   editorGroup);
    insertTlmBtn_   = new QPushButton("+ TELEMETRY", editorGroup);
    insertParamBtn_ = new QPushButton("+ PARAMETER", editorGroup);
    addFieldBtn_    = new QPushButton("Add Field...", editorGroup);
    insertCmdBtn_->hide();
    insertTlmBtn_->hide();
    insertParamBtn_->hide();
    addFieldBtn_->hide();

    auto* insertBar = new QHBoxLayout;
    insertMenuBtn_ = new QToolButton(editorGroup);
    insertMenuBtn_->setText("Insert " + QString::fromUtf8("▾"));
    insertMenuBtn_->setPopupMode(QToolButton::InstantPopup);
    insertMenuBtn_->setVisible(false);
    auto* insertMenu = new QMenu(insertMenuBtn_);
    insertCmdAction_   = insertMenu->addAction("COMMAND");
    insertTlmAction_   = insertMenu->addAction("TELEMETRY");
    insertParamAction_ = insertMenu->addAction("PARAMETER");
    addFieldAction_    = insertMenu->addAction("Add Field...");
    insertMenuBtn_->setMenu(insertMenu);
    insertBar->addWidget(insertMenuBtn_);
    insertBar->addStretch();
    editorLayout->addLayout(insertBar);

    editor_ = new QPlainTextEdit(editorGroup);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editor_->setFont(mono);
    editor_->setObjectName("LogArea");
    highlighter_ = new Widgets::CmdTlmHighlighter(editor_->document());
    editorLayout->addWidget(editor_, 1);

    auto* guideGroup = new QGroupBox("Reference", editorGroup);
    auto* guideLayout = new QVBoxLayout(guideGroup);
    syntaxGuide_ = new QTextEdit(guideGroup);
    syntaxGuide_->setObjectName("LogArea");
    syntaxGuide_->setReadOnly(true);
    syntaxGuide_->setLineWrapMode(QTextEdit::WidgetWidth);
    syntaxGuide_->setPlainText(syntaxGuideText());
    guideLayout->addWidget(syntaxGuide_);
    guideGroup->setVisible(false);
    editorLayout->addWidget(guideGroup);
    connect(referenceBtn, &QPushButton::toggled, guideGroup, &QWidget::setVisible);

    hSplit->addWidget(fileGroup);
    hSplit->addWidget(editorGroup);
    hSplit->setChildrenCollapsible(false); // dragging must not make a pane vanish
    hSplit->setStretchFactor(0, 0);
    hSplit->setStretchFactor(1, 1);        // editor absorbs extra width
    hSplit->setSizes({240, 880});
    fileGroup->setMinimumWidth(160);
    editorGroup->setMinimumWidth(420);
    vSplit->addWidget(hSplit);

    // ── Bottom tabs: structure + diagnostics ─────────────────────────────────
    auto* bottomTabs = new QTabWidget(vSplit);

    auto* structPage  = new QWidget(bottomTabs);
    auto* structLayout = new QVBoxLayout(structPage);
    structureTree_ = new QTreeWidget(structPage);
    structureTreeEmptyLabel_ = new QLabel("Open and validate a CMD/TLM .txt file to see its structure.", structPage);
    structureTreeEmptyLabel_->setObjectName("SubLabel");
    structureTreeEmptyLabel_->setWordWrap(true);
    structureTree_->setHeaderLabels({"Name", "Type", "Bits", "Description"});
    structureTree_->header()->setStretchLastSection(true);
    structureTree_->setRootIsDecorated(true);
    structureTree_->setAlternatingRowColors(true);
    structLayout->addWidget(structureTreeEmptyLabel_);
    structLayout->addWidget(structureTree_);
    bottomTabs->addTab(structPage, "Structure");

    auto* diagPage  = new QWidget(bottomTabs);
    auto* diagLayout = new QVBoxLayout(diagPage);
    diagSummary_ = new QLabel("", diagPage);
    diagSummary_->setObjectName("SubLabel");
    diagnosticListEmptyLabel_ = new QLabel("Validate an open CMD/TLM .txt file to show diagnostics.", diagPage);
    diagnosticListEmptyLabel_->setObjectName("SubLabel");
    diagnosticListEmptyLabel_->setWordWrap(true);
    diagnosticList_ = new QListWidget(diagPage);
    diagnosticList_->setMaximumHeight(120);
    diagLayout->addWidget(diagSummary_);
    diagLayout->addWidget(diagnosticListEmptyLabel_);
    diagLayout->addWidget(diagnosticList_);
    bottomTabs->addTab(diagPage, "Diagnostics");
    vSplit->addWidget(bottomTabs);

    vSplit->setChildrenCollapsible(false); // keep editor and bottom tabs visible on drag
    vSplit->setStretchFactor(0, 1);        // editor row grows first
    vSplit->setStretchFactor(1, 0);
    vSplit->setSizes({560, 180});
    root->addWidget(vSplit, 1);

    // ── Status bar ────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel(
        "Open a .txt CMD/TLM file to enable insert buttons.", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    // Initial path
    pathEdit_->setText("/cosmos/targets");
}

// ── bindViewModel ─────────────────────────────────────────────────────────────

void CmdTlmView::bindViewModel()
{
    connect(browseBtn_,     &QPushButton::clicked, this, &CmdTlmView::onBrowseClicked);
    connect(saveBtn_,       &QPushButton::clicked, this, &CmdTlmView::onSaveClicked);

    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] {
        if (saveBtn_->isEnabled()) onSaveClicked();
    });
    connect(validateBtn_,   &QPushButton::clicked, this, &CmdTlmView::onValidateClicked);
    connect(openInValidatorBtn_, &QPushButton::clicked, this, &CmdTlmView::onOpenInValidator);
    connect(insertCmdBtn_,  &QPushButton::clicked, this, &CmdTlmView::onInsertCmd);
    connect(insertTlmBtn_,  &QPushButton::clicked, this, &CmdTlmView::onInsertTlm);
    connect(insertParamBtn_,&QPushButton::clicked, this, &CmdTlmView::onInsertParam);
    connect(addFieldBtn_,   &QPushButton::clicked, this, &CmdTlmView::onAddField);
    connect(insertCmdAction_,   &QAction::triggered, this, &CmdTlmView::onInsertCmd);
    connect(insertTlmAction_,   &QAction::triggered, this, &CmdTlmView::onInsertTlm);
    connect(insertParamAction_, &QAction::triggered, this, &CmdTlmView::onInsertParam);
    connect(addFieldAction_,    &QAction::triggered, this, &CmdTlmView::onAddField);

    connect(fileList_, &QListWidget::itemDoubleClicked,
            this, &CmdTlmView::onFileItemDoubleClicked);

    connect(structureTree_, &QTreeWidget::itemClicked,
            this, &CmdTlmView::onStructureItemClicked);

    connect(diagnosticList_, &QListWidget::itemClicked,
            this, &CmdTlmView::onDiagnosticItemClicked);

    connect(&vm_, &ViewModels::CmdTlmViewModel::connectionChanged,
            this, [this] {
                const bool connected = vm_.isConnected();
                connectionHint_->setVisible(!connected);
                browseBtn_->setEnabled(connected);
                pathEdit_->setEnabled(connected);
                if (connected)
                    pathEdit_->setText(vm_.defaultCmdTlmPath());
                updateActionHints();
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::busyChanged,
            this, [this] {
                const bool connected = vm_.isConnected();
                const bool busy = vm_.isBusy();
                browseBtn_->setEnabled(connected && !busy);
                updateActionHints();
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::statusMessageChanged,
            this, [this] {
                const QString message = vm_.statusMessage();
                if (message.trimmed().isEmpty())
                    return;
                statusLabel_->setText(QString("Status: %1").arg(message));
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::directoryListed,
            this, [this](const QStringList& entries, const QString& path) {
                currentDir_ = path;
                fileList_->clear();
                for (const QString& e : entries)
                    fileList_->addItem(e);
                updateEmptyStateLabels();
                updateActionHints();
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileOpened,
            this, [this](const QString& path, const QString& content) {
                currentFile_ = path;
                fileLabel_->setText(path);
                editor_->setPlainText(content);
                editor_->document()->setModified(false);
                const bool isTxt = path.endsWith(".txt", Qt::CaseInsensitive);
                saveBtn_->setEnabled(true);
                validateBtn_->setEnabled(isTxt);
                openInValidatorBtn_->setEnabled(isTxt);
                insertMenuBtn_->setVisible(isTxt);
                insertMenuBtn_->setEnabled(isTxt);
                updateActionHints();
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileSaved,
            this, [this](const QString& /*path*/, bool ok) {
                if (!ok) {
                    statusLabel_->setText("Error: Save failed. Could not write the file to the remote host.");
                    QMessageBox::warning(this, "Save Failed",
                        "Could not write the file to the remote host.");
                } else {
                    statusLabel_->setText("OK: File saved successfully.");
                    editor_->document()->setModified(false);
                }
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileParsed,
            this, &CmdTlmView::onFileParsed);

    // Initial state
    const bool connected = vm_.isConnected();
    connectionHint_->setVisible(!connected);
    browseBtn_->setEnabled(connected);
    pathEdit_->setEnabled(connected);
    if (connected) pathEdit_->setText(vm_.defaultCmdTlmPath());
    updateEmptyStateLabels();
    updateActionHints();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void CmdTlmView::openDirectory(const QString& remotePath)
{
    const QString path = remotePath.trimmed();
    if (path.isEmpty()) return;
    currentDir_ = path;
    pathEdit_->setText(path);
    vm_.listDirectory(path);
}

bool CmdTlmView::confirmDiscardUnsavedChanges()
{
    if (currentFile_.isEmpty() || !editor_->document()->isModified())
        return true;

    const auto choice = QMessageBox::question(
        this, "Unsaved Changes",
        "Save changes to " + currentFile_ + " before opening a new file?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save)
        vm_.saveFile(currentFile_, editor_->toPlainText());
    return true;
}

void CmdTlmView::openFile(const QString& remotePath)
{
    const QString path = remotePath.trimmed();
    if (path.isEmpty()) return;
    if (!confirmDiscardUnsavedChanges()) return;

    const qsizetype slash = path.lastIndexOf('/');
    if (slash > 0) {
        currentDir_ = path.left(slash);
        pathEdit_->setText(currentDir_);
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
        if (!confirmDiscardUnsavedChanges()) return;
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

void CmdTlmView::onOpenInValidator()
{
    emit openInValidatorRequested(editor_->toPlainText());
}

void CmdTlmView::onInsertCmd()   { insertTextAtCursor(Snippets::command()); }
void CmdTlmView::onInsertTlm()   { insertTextAtCursor(Snippets::telemetry()); }
void CmdTlmView::onInsertParam() { insertTextAtCursor(Snippets::parameter()); }

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
    const int e = result.errorCount();
    const int w = result.warningCount();
    if (e > 0) {
        statusLabel_->setText(QString("Error: Validation found %1 error(s) and %2 warning(s).").arg(e).arg(w));
    } else if (w > 0) {
        statusLabel_->setText(QString("Warning: Validation found %1 warning(s).").arg(w));
    } else {
        statusLabel_->setText("OK: Validation completed with no issues.");
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

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
    updateEmptyStateLabels();
}

void CmdTlmView::populateDiagnostics(const ViewModels::CmdTlmParseResult& result)
{
    diagnosticList_->clear();

    const int e = result.errorCount();
    const int w = result.warningCount();

    if (result.diagnostics.isEmpty()) {
        diagSummary_->setText("OK: No issues found.");
    } else {
        diagSummary_->setText(QString("Error: %1 error(s)   Warning: %2 warning(s)").arg(e).arg(w));
    }

    for (const auto& d : result.diagnostics) {
        const bool isError = (d.severity == ViewModels::CmdTlmDiagnostic::Severity::Error);
        const QString text = QString("%1 Line %2: %3")
            .arg(isError ? "Error" : "Warning")
            .arg(d.line)
            .arg(d.message);

        auto* li = new QListWidgetItem(text, diagnosticList_);
        li->setData(Qt::UserRole, d.line);
        li->setForeground(isError ? QColor("#F44747") : QColor("#CCA700"));
    }
    updateEmptyStateLabels();
}

void CmdTlmView::updateActionHints()
{
    const bool connected = vm_.isConnected();
    const bool busy = vm_.isBusy();
    const bool hasFile = !currentFile_.isEmpty();
    const bool isTxt = currentFile_.endsWith(".txt", Qt::CaseInsensitive);
    const QString connectReason = "Connect to an OpenC3 environment first.";
    const QString fileReason = "Open a CMD/TLM .txt file first.";
    const QString busyReason = "Wait for the current CMD/TLM operation to finish.";

    browseBtn_->setToolTip(connected ? "Browse the selected remote directory." : connectReason);
    const QString editTip = !connected ? connectReason : (!isTxt ? fileReason : "Insert a COMMAND/TELEMETRY template or a field.");
    insertMenuBtn_->setToolTip(editTip);
    validateBtn_->setToolTip("Check the open CMD/TLM file.");
    openInValidatorBtn_->setToolTip("Run full offline validation in the Validator view.");
    saveBtn_->setToolTip(!hasFile ? fileReason : (busy ? busyReason : "Save the open file."));

    if (!connected)
        statusLabel_->setText(connectReason);
    else if (!hasFile)
        statusLabel_->setText("Open a file, then edit and save.");
    else if (!isTxt)
        statusLabel_->setText("This file isn't CMD/TLM - edit and save only.");
    else if (editor_->document()->isModified())
        statusLabel_->setText("Unsaved changes. Check, then Save.");
    else
        statusLabel_->setText("Saved. Check it before moving on, if needed.");
}

void CmdTlmView::updateEmptyStateLabels()
{
    fileListEmptyLabel_->setVisible(fileList_->count() == 0);
    structureTreeEmptyLabel_->setVisible(structureTree_->topLevelItemCount() == 0);
    diagnosticListEmptyLabel_->setVisible(diagnosticList_->count() == 0);
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
