#include "CmdTlmView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QFontDatabase>

namespace OpenC3::UI::Views {

CmdTlmView::CmdTlmView(ViewModels::CmdTlmViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void CmdTlmView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("CMD / TLM Editor", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Connection hint ───────────────────────────────────────────────────────
    connectionHint_ = new QLabel(
        "Connect to a remote environment to browse and edit files.", this);
    connectionHint_->setObjectName("SubLabel");
    root->addWidget(connectionHint_);

    // ── Path bar ──────────────────────────────────────────────────────────────
    auto* pathBar = new QHBoxLayout;
    pathEdit_ = new QLineEdit("/cosmos/config/targets", this);
    listBtn_  = new QPushButton("Browse", this);
    pathBar->addWidget(new QLabel("Path:", this));
    pathBar->addWidget(pathEdit_);
    pathBar->addWidget(listBtn_);
    root->addLayout(pathBar);

    // ── Splitter: file list / editor ──────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    fileList_ = new QListWidget(splitter);
    fileList_->setObjectName("fileList");

    auto* editorGroup  = new QGroupBox("Editor", splitter);
    auto* editorLayout = new QVBoxLayout(editorGroup);

    fileLabel_ = new QLabel("(no file open)", editorGroup);
    fileLabel_->setObjectName("SubLabel");

    editor_ = new QTextEdit(editorGroup);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editor_->setFont(mono);
    editor_->setObjectName("LogArea");

    auto* editorBtns = new QHBoxLayout;
    saveBtn_ = new QPushButton("💾  Save", editorGroup);
    saveBtn_->setObjectName("PrimaryButton");
    saveBtn_->setEnabled(false);
    editorBtns->addStretch();
    editorBtns->addWidget(saveBtn_);

    editorLayout->addWidget(fileLabel_);
    editorLayout->addWidget(editor_);
    editorLayout->addLayout(editorBtns);

    splitter->addWidget(fileList_);
    splitter->addWidget(editorGroup);
    splitter->setSizes({280, 900});
    root->addWidget(splitter);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);
}

void CmdTlmView::bindViewModel()
{
    connect(listBtn_, &QPushButton::clicked, this, &CmdTlmView::onListClicked);

    connect(fileList_, &QListWidget::itemDoubleClicked,
            this, &CmdTlmView::onFileItemDoubleClicked);

    connect(saveBtn_, &QPushButton::clicked, this, &CmdTlmView::onSaveClicked);

    connect(&vm_, &ViewModels::CmdTlmViewModel::connectionChanged,
            this, [this] {
                const bool on = vm_.isConnected();
                connectionHint_->setVisible(!on);
                listBtn_->setEnabled(on);
                pathEdit_->setEnabled(on);
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::busyChanged,
            this, [this] {
                listBtn_->setEnabled(vm_.isConnected() && !vm_.isBusy());
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
                saveBtn_->setEnabled(true);
            });

    connect(&vm_, &ViewModels::CmdTlmViewModel::fileSaved,
            this, [this](const QString& /*path*/, bool ok) {
                if (!ok)
                    QMessageBox::warning(this, "Save Failed",
                        "Could not write the file to the remote host.");
            });

    // Initial state
    const bool on = vm_.isConnected();
    connectionHint_->setVisible(!on);
    listBtn_->setEnabled(on);
    pathEdit_->setEnabled(on);
}

void CmdTlmView::onListClicked()
{
    vm_.listDirectory(pathEdit_->text().trimmed());
}

void CmdTlmView::onFileItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString name = item->text();
    const QString path = currentDir_.endsWith('/') || currentDir_.isEmpty()
        ? currentDir_ + name
        : currentDir_ + '/' + name;

    if (name.endsWith('/') || !name.contains('.')) {
        // Navigate into directory
        pathEdit_->setText(path.chopped(name.endsWith('/') ? 1 : 0));
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

} // namespace OpenC3::UI::Views
