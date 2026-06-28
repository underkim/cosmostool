#include "PluginView.h"
#include "ui/dialogs/AddTargetDialog.h"
#include "ui/dialogs/CmdTlmFieldDialog.h"
#include "ui/dialogs/PluginWizard.h"
#include "ui/widgets/CmdTlmHighlighter.h"

#include <QAbstractItemView>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

namespace OpenC3::UI::Views {

namespace {

const QString kCmdTemplate =
    "COMMAND TARGET COMMAND_NAME BIG_ENDIAN \"Command description\"\n"
    "  PARAMETER PARAM_NAME 0 16 UINT 0 65535 0 \"Parameter description\"\n";

const QString kTlmTemplate =
    "TELEMETRY TARGET PACKET_NAME BIG_ENDIAN \"Telemetry description\"\n"
    "  ITEM ITEM_NAME 0 16 UINT \"Item description\"\n";

bool isCmdTlmFile(const QString& path)
{
    return path.contains("/cmd_tlm/", Qt::CaseInsensitive)
        || path.contains("\\cmd_tlm\\", Qt::CaseInsensitive);
}

QString componentKind(const QString& path)
{
    if (path == "plugin.txt") return "Plugin";
    if (path.endsWith(".gemspec", Qt::CaseInsensitive)) return "Gemspec";
    if (path.contains("/cmd_tlm/", Qt::CaseInsensitive)) return "CMD/TLM";
    if (path.contains("/screens/", Qt::CaseInsensitive)) return "Screen";
    if (path.contains("/procedures/", Qt::CaseInsensitive)) return "Procedure";
    return "File";
}

QString syntaxGuideText()
{
    return QStringLiteral(
        "CMD/TLM Quick Reference\n\n"
        "COMMAND <TARGET> <NAME> <ENDIANNESS> \"description\"\n"
        "TELEMETRY <TARGET> <NAME> <ENDIANNESS> \"description\"\n\n"
        "APPEND_PARAMETER <NAME> <BIT_SIZE> <TYPE> <MIN> <MAX> <DEFAULT> \"description\"\n"
        "  Command input field. Use APPEND_ID_PARAMETER for command IDs.\n\n"
        "APPEND_ITEM <NAME> <BIT_SIZE> <TYPE> \"description\"\n"
        "  Telemetry output field. Use APPEND_ID_ITEM for packet IDs.\n\n"
        "Use PARAMETER or ITEM when you need to provide an explicit bit offset.\n\n"
        "Types\n"
        "  UINT, INT, FLOAT\n"
        "  UINT8, UINT16, UINT32, UINT64\n"
        "  INT8, INT16, INT32, INT64\n"
        "  FLOAT32, FLOAT64\n"
        "  STRING, BLOCK, DERIVED\n\n"
        "Child keywords\n"
        "  STATE, UNITS, FORMAT_STRING\n"
        "  REQUIRED, HIDDEN, HAZARDOUS\n"
        "  LIMITS, LIMITS_RESPONSE\n"
        "  READ_CONVERSION, WRITE_CONVERSION\n\n"
        "Example\n"
        "  TELEMETRY FSW HK BIG_ENDIAN \"Housekeeping\"\n"
        "    APPEND_ITEM TEMP 16 INT \"Temperature\"\n"
        "      UNITS Celsius C\n");
}

} // namespace

PluginView::PluginView(
    ViewModels::PluginViewModel& vm,
    ViewModels::InfraViewModel& infraVm,
    ViewModels::CmdTlmViewModel& cmdTlmVm,
    QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
    , infraVm_(infraVm)
    , cmdTlmVm_(cmdTlmVm)
{
    setupUi();
    bindViewModel();
}

void PluginView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    auto* title = new QLabel("Plugin Manager", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setObjectName("PageTitle");
    root->addWidget(title);

    auto* toolbar = new QHBoxLayout;
    refreshBtn_ = new QPushButton("Refresh", this);
    installBtn_ = new QPushButton("Install Gem", this);
    removeBtn_ = new QPushButton("Remove", this);
    validateBtn_ = new QPushButton("Validate", this);
    buildBtn_ = new QPushButton("Build", this);
    scaffoldBtn_ = new QPushButton("New Plugin", this);
    addTargetBtn_ = new QPushButton("Add Target", this);

    scaffoldBtn_->setToolTip("Create a new OpenC3 plugin folder structure.");
    addTargetBtn_->setToolTip("Add a target folder structure to the selected plugin.");

    removeBtn_->setEnabled(false);
    buildBtn_->setEnabled(false);
    addTargetBtn_->setEnabled(false);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    progressBar_->setFixedWidth(120);

    toolbar->addWidget(refreshBtn_);
    toolbar->addSpacing(8);
    toolbar->addWidget(installBtn_);
    toolbar->addWidget(removeBtn_);
    toolbar->addWidget(validateBtn_);
    toolbar->addWidget(buildBtn_);
    toolbar->addSpacing(16);
    toolbar->addWidget(scaffoldBtn_);
    toolbar->addWidget(addTargetBtn_);
    toolbar->addStretch();
    toolbar->addWidget(progressBar_);
    root->addLayout(toolbar);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    auto* mainSplitter = new QSplitter(Qt::Vertical, this);

    tableView_ = new QTableView(mainSplitter);
    tableView_->setModel(vm_.pluginModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setShowGrid(false);
    tableView_->setSortingEnabled(true);

    detailTabs_ = new QTabWidget(mainSplitter);

    auto* detailTab = new QWidget(detailTabs_);
    auto* detailLayout = new QVBoxLayout(detailTab);
    detailLayout->setContentsMargins(8, 8, 8, 8);
    detailEdit_ = new QTextEdit(detailTab);
    detailEdit_->setReadOnly(true);
    detailEdit_->setObjectName("LogArea");
    detailLayout->addWidget(detailEdit_);
    detailTabs_->addTab(detailTab, "Details");

    auto* componentTab = new QWidget(detailTabs_);
    auto* componentLayout = new QVBoxLayout(componentTab);
    componentLayout->setContentsMargins(8, 8, 8, 8);
    componentLayout->setSpacing(8);

    auto* componentSplitter = new QSplitter(Qt::Horizontal, componentTab);
    componentList_ = new QListWidget(componentSplitter);
    componentList_->setObjectName("PluginComponentList");
    componentList_->setMinimumWidth(260);

    auto* editorPane = new QWidget(componentSplitter);
    auto* editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    auto* componentToolbar = new QHBoxLayout;
    componentPathLabel_ = new QLabel("Select a plugin file", editorPane);
    componentPathLabel_->setObjectName("SubLabel");
    openComponentBtn_ = new QPushButton("Open File", editorPane);
    saveComponentBtn_ = new QPushButton("Save", editorPane);
    validateComponentBtn_ = new QPushButton("Validate CMD/TLM", editorPane);
    openInCmdTlmBtn_ = new QPushButton("Open in CMD/TLM", editorPane);
    insertCmdBtn_ = new QPushButton("+ COMMAND", editorPane);
    insertTlmBtn_ = new QPushButton("+ TELEMETRY", editorPane);
    addFieldBtn_ = new QPushButton("Add Field...", editorPane);
    openComponentBtn_->setEnabled(false);
    saveComponentBtn_->setEnabled(false);
    validateComponentBtn_->setEnabled(false);
    openInCmdTlmBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);

    componentToolbar->addWidget(componentPathLabel_, 1);
    componentToolbar->addWidget(openComponentBtn_);
    componentToolbar->addWidget(insertCmdBtn_);
    componentToolbar->addWidget(insertTlmBtn_);
    componentToolbar->addWidget(addFieldBtn_);
    componentToolbar->addWidget(validateComponentBtn_);
    componentToolbar->addWidget(openInCmdTlmBtn_);
    componentToolbar->addWidget(saveComponentBtn_);
    editorLayout->addLayout(componentToolbar);

    componentEditor_ = new QTextEdit(editorPane);
    componentEditor_->setObjectName("CodeEditor");
    componentEditor_->setLineWrapMode(QTextEdit::NoWrap);
    highlighter_ = new Widgets::CmdTlmHighlighter(componentEditor_->document());
    editorLayout->addWidget(componentEditor_, 1);

    componentDiagnostics_ = new QTextEdit(editorPane);
    componentDiagnostics_->setObjectName("LogArea");
    componentDiagnostics_->setReadOnly(true);
    componentDiagnostics_->setMaximumHeight(120);
    editorLayout->addWidget(componentDiagnostics_);

    auto* guideGroup = new QGroupBox("Reference", componentSplitter);
    auto* guideLayout = new QVBoxLayout(guideGroup);
    componentGuide_ = new QTextEdit(guideGroup);
    componentGuide_->setObjectName("LogArea");
    componentGuide_->setReadOnly(true);
    componentGuide_->setLineWrapMode(QTextEdit::WidgetWidth);
    componentGuide_->setPlainText(syntaxGuideText());
    guideLayout->addWidget(componentGuide_);

    componentSplitter->addWidget(componentList_);
    componentSplitter->addWidget(editorPane);
    componentSplitter->addWidget(guideGroup);
    componentSplitter->setStretchFactor(0, 0);
    componentSplitter->setStretchFactor(1, 1);
    componentSplitter->setStretchFactor(2, 0);
    componentSplitter->setSizes({280, 680, 300});
    componentLayout->addWidget(componentSplitter);
    detailTabs_->addTab(componentTab, "Component Files");

    mainSplitter->addWidget(tableView_);
    mainSplitter->addWidget(detailTabs_);
    mainSplitter->setSizes({360, 360});
    root->addWidget(mainSplitter);
}

void PluginView::bindViewModel()
{
    connect(refreshBtn_, &QPushButton::clicked, &vm_, &ViewModels::PluginViewModel::refresh);
    connect(installBtn_, &QPushButton::clicked, this, &PluginView::onInstallClicked);
    connect(removeBtn_, &QPushButton::clicked, this, &PluginView::onRemoveClicked);
    connect(validateBtn_, &QPushButton::clicked, this, &PluginView::onValidateClicked);
    connect(buildBtn_, &QPushButton::clicked, this, &PluginView::onBuildClicked);
    connect(scaffoldBtn_, &QPushButton::clicked, this, &PluginView::onScaffoldClicked);
    connect(addTargetBtn_, &QPushButton::clicked, this, &PluginView::onAddTargetClicked);
    connect(openComponentBtn_, &QPushButton::clicked, this, &PluginView::onOpenComponentClicked);
    connect(saveComponentBtn_, &QPushButton::clicked, this, &PluginView::onSaveComponentClicked);
    connect(validateComponentBtn_, &QPushButton::clicked, this, &PluginView::onValidateComponentClicked);
    connect(openInCmdTlmBtn_, &QPushButton::clicked, this, &PluginView::onOpenInCmdTlmClicked);
    connect(insertCmdBtn_, &QPushButton::clicked, this, &PluginView::onInsertCmdClicked);
    connect(insertTlmBtn_, &QPushButton::clicked, this, &PluginView::onInsertTlmClicked);
    connect(addFieldBtn_, &QPushButton::clicked, this, &PluginView::onAddFieldClicked);

    connect(componentList_, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item) {
                if (!item) return;
                componentPathLabel_->setText("Selected: " + item->data(Qt::UserRole + 1).toString());
                openComponentBtn_->setEnabled(true);
            });
    connect(componentList_, &QListWidget::itemDoubleClicked,
            this, &PluginView::onComponentItemDoubleClicked);

    connect(tableView_->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, &PluginView::onTableSelectionChanged);

    connect(&infraVm_, &ViewModels::InfraViewModel::busyChanged,
            this, [this] {
                const bool busy = infraVm_.isBusy();
                scaffoldBtn_->setEnabled(!busy);
                addTargetBtn_->setEnabled(!busy && tableView_->selectionModel()->hasSelection());
            });

    connect(&infraVm_, &ViewModels::InfraViewModel::scaffoldComplete,
            this, [this](const QString& rootPath, bool success, const QString& detail) {
                detailTabs_->setCurrentIndex(0);
                detailEdit_->setPlainText(detail);
                if (success) {
                    currentPluginRoot_ = rootPath;
                    vm_.refresh();
                    cmdTlmVm_.listPluginFiles(rootPath);
                }
            });

    connect(&infraVm_, &ViewModels::InfraViewModel::targetAdded,
            this, [this](const QString&, bool success, const QString& detail) {
                detailTabs_->setCurrentIndex(0);
                detailEdit_->setPlainText(detail);
                if (success) {
                    vm_.refresh();
                    const QString root = selectedPluginRoot();
                    if (!root.isEmpty())
                        cmdTlmVm_.listPluginFiles(root);
                }
            });

    connect(&vm_, &ViewModels::PluginViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy() || cmdTlmVm_.isBusy();
                progressBar_->setVisible(busy);
                refreshBtn_->setEnabled(!busy);
                installBtn_->setEnabled(!busy);
                validateBtn_->setEnabled(!busy);
                buildBtn_->setEnabled(!busy && tableView_->selectionModel()->hasSelection());
                removeBtn_->setEnabled(!busy && tableView_->selectionModel()->hasSelection());
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy() || cmdTlmVm_.isBusy();
                progressBar_->setVisible(busy);
                saveComponentBtn_->setEnabled(!busy && !currentComponentPath_.isEmpty());
                openComponentBtn_->setEnabled(!busy && componentList_->currentItem() != nullptr);
                validateComponentBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                openInCmdTlmBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                addFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
            });

    connect(&vm_, &ViewModels::PluginViewModel::statusMessageChanged,
            this, [this] {
                statusLabel_->setText(vm_.statusMessage());
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::statusMessageChanged,
            this, [this] {
                if (!cmdTlmVm_.statusMessage().isEmpty())
                    statusLabel_->setText(cmdTlmVm_.statusMessage());
            });

    connect(&vm_, &ViewModels::PluginViewModel::validationComplete,
            this, [this](bool valid, const QString& summary) {
                detailTabs_->setCurrentIndex(0);
                detailEdit_->setPlainText(summary);
                if (!valid)
                    QMessageBox::warning(this, "Plugin Manager", summary);
            });

    connect(&vm_, &ViewModels::PluginViewModel::actionCompleted,
            this, [this](const QString& name, bool ok) {
                if (!ok)
                    QMessageBox::warning(this, "Plugin Manager",
                        "Action failed for: " + name);
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::pluginFilesListed,
            this, &PluginView::populateComponentList);

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileOpened,
            this, [this](const QString& path, const QString& content) {
                currentComponentPath_ = path;
                componentPathLabel_->setText(path);
                componentEditor_->setPlainText(content);
                saveComponentBtn_->setEnabled(true);
                const bool cmdTlm = isCmdTlmFile(path);
                validateComponentBtn_->setEnabled(cmdTlm);
                openInCmdTlmBtn_->setEnabled(cmdTlm);
                insertCmdBtn_->setEnabled(cmdTlm);
                insertTlmBtn_->setEnabled(cmdTlm);
                addFieldBtn_->setEnabled(cmdTlm);
                componentDiagnostics_->setPlainText(
                    cmdTlm ? "Loaded CMD/TLM definition." : "Loaded text file.");
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileSaved,
            this, [this](const QString& path, bool success) {
                componentDiagnostics_->setPlainText(
                    success ? "Saved: " + path : "Save failed: " + path);
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileParsed,
            this, [this](const ViewModels::CmdTlmParseResult& result, const QString& filePath) {
                if (filePath != currentComponentPath_ || !isCmdTlmFile(filePath))
                    return;

                QStringList lines;
                lines << QString("Blocks: %1, Errors: %2, Warnings: %3")
                             .arg(result.blocks.size())
                             .arg(result.errorCount())
                             .arg(result.warningCount());
                for (const auto& d : result.diagnostics) {
                    lines << QString("Line %1: %2").arg(d.line).arg(d.message);
                }
                componentDiagnostics_->setPlainText(lines.join('\n'));
            });
}

void PluginView::onInstallClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Plugin Gem", {}, "Gem files (*.gem);;All files (*)");
    if (!path.isEmpty())
        vm_.install(path);
}

void PluginView::onRemoveClicked()
{
    const QString name = selectedPluginName();
    if (name.isEmpty()) return;
    if (QMessageBox::question(this, "Confirm Remove",
            "Remove plugin: " + name + "?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        vm_.remove(name);
}

void PluginView::onScaffoldClicked()
{
    Dialogs::PluginWizard wizard(infraVm_, this);
    if (wizard.exec() == QDialog::Accepted)
        vm_.refresh();
}

void PluginView::onAddTargetClicked()
{
    const QString root = selectedPluginRoot();
    if (root.isEmpty()) {
        QMessageBox::information(this, "Add Target",
            "Select a plugin folder before adding a target.");
        return;
    }
    Dialogs::AddTargetDialog dlg(infraVm_, root, this);
    if (dlg.exec() == QDialog::Accepted) {
        vm_.refresh();
        cmdTlmVm_.listPluginFiles(root);
    }
}

void PluginView::onValidateClicked()
{
    const QString root = selectedPluginRoot();
    if (!root.isEmpty()) {
        vm_.validate(root);
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, "Select Plugin Directory or Gem", {}, "All files (*)");
    if (!path.isEmpty()) vm_.validate(path);
}

void PluginView::onBuildClicked()
{
    const QString root = selectedPluginRoot();
    if (root.isEmpty()) {
        QMessageBox::information(this, "Build Plugin",
            "Select a plugin folder first.");
        return;
    }
    vm_.build(root);
}

void PluginView::onTableSelectionChanged()
{
    const bool hasSel = tableView_->selectionModel()->hasSelection();
    removeBtn_->setEnabled(hasSel && !vm_.isBusy());
    buildBtn_->setEnabled(hasSel && !vm_.isBusy());
    addTargetBtn_->setEnabled(hasSel && !infraVm_.isBusy());

    componentList_->clear();
    componentEditor_->clear();
    componentDiagnostics_->clear();
    currentPluginRoot_.clear();
    currentComponentPath_.clear();
    componentPathLabel_->setText(hasSel ? "Loading plugin files" : "Select a plugin file");
    saveComponentBtn_->setEnabled(false);
    openComponentBtn_->setEnabled(false);
    validateComponentBtn_->setEnabled(false);
    openInCmdTlmBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);

    if (!hasSel) {
        detailEdit_->clear();
        return;
    }

    const auto* plugin =
        vm_.pluginModel()->pluginAt(tableView_->selectionModel()->selectedRows().first().row());
    if (!plugin) return;

    const QString detail = QString(
        "Name:       %1\n"
        "Root:       %2\n"
        "plugin.txt: %3\n"
        "gemspec:    %4\n"
        "gem:        %5\n"
        "Version:    %6\n"
        "Author:     %7\n"
        "COSMOS:     %8\n\n"
        "%9\n\n"
        "Targets:\n%10")
        .arg(QString::fromStdString(plugin->name))
        .arg(QString::fromStdString(plugin->rootPath))
        .arg(QString::fromStdString(plugin->pluginTxtPath))
        .arg(QString::fromStdString(plugin->gemspecPath))
        .arg(QString::fromStdString(plugin->gemFilePath))
        .arg(QString::fromStdString(plugin->version))
        .arg(QString::fromStdString(plugin->author))
        .arg(QString::fromStdString(plugin->cosmosVersion))
        .arg(QString::fromStdString(plugin->description))
        .arg([&]{
            QStringList targets;
            for (const auto& target : plugin->targets)
                targets << QString("  %1").arg(QString::fromStdString(target.name));
            return targets.isEmpty() ? "  (none)" : targets.join('\n');
        }());

    detailEdit_->setPlainText(detail);
    currentPluginRoot_ = selectedPluginRoot();
    if (!currentPluginRoot_.isEmpty())
        cmdTlmVm_.listPluginFiles(currentPluginRoot_);
}

void PluginView::onComponentItemDoubleClicked(QListWidgetItem* item)
{
    openSelectedComponent(item);
}

void PluginView::onOpenComponentClicked()
{
    openSelectedComponent(componentList_->currentItem());
}

void PluginView::onSaveComponentClicked()
{
    if (currentComponentPath_.isEmpty()) return;
    cmdTlmVm_.saveFile(currentComponentPath_, componentEditor_->toPlainText());
}

void PluginView::onValidateComponentClicked()
{
    if (currentComponentPath_.isEmpty()) return;
    if (!isCmdTlmFile(currentComponentPath_)) {
        componentDiagnostics_->setPlainText("CMD/TLM validation is available for cmd_tlm files.");
        return;
    }
    cmdTlmVm_.parseContent(componentEditor_->toPlainText(), currentComponentPath_);
}

void PluginView::onOpenInCmdTlmClicked()
{
    if (currentComponentPath_.isEmpty()) return;
    if (!isCmdTlmFile(currentComponentPath_)) {
        componentDiagnostics_->setPlainText("Open in CMD/TLM is available for cmd_tlm files.");
        return;
    }
    emit openCmdTlmRequested(currentComponentPath_);
}

void PluginView::onInsertCmdClicked()
{
    insertTextAtCursor(kCmdTemplate);
}

void PluginView::onInsertTlmClicked()
{
    insertTextAtCursor(kTlmTemplate);
}

void PluginView::onAddFieldClicked()
{
    Dialogs::CmdTlmFieldDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        insertTextAtCursor(dialog.generatedLine());
}

void PluginView::populateComponentList(const QStringList& files, const QString& pluginRootPath)
{
    if (pluginRootPath != currentPluginRoot_)
        return;

    componentList_->clear();
    for (const QString& relPath : files) {
        auto* item = new QListWidgetItem(
            QString("[%1] %2").arg(componentKind(relPath), relPath),
            componentList_);
        item->setData(Qt::UserRole, pluginRootPath + "/" + relPath);
        item->setData(Qt::UserRole + 1, relPath);
        componentList_->addItem(item);
    }

    componentPathLabel_->setText(
        files.isEmpty() ? "No plugin component files found" : "Select a plugin file");
}

void PluginView::openSelectedComponent(QListWidgetItem* item)
{
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    cmdTlmVm_.openFile(path);
}

void PluginView::insertTextAtCursor(const QString& text)
{
    auto cursor = componentEditor_->textCursor();
    cursor.insertText(text);
    componentEditor_->setTextCursor(cursor);
}

QString PluginView::selectedPluginName() const
{
    const auto rows = tableView_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* p = vm_.pluginModel()->pluginAt(rows.first().row());
    return p ? QString::fromStdString(p->name) : QString{};
}

QString PluginView::selectedPluginRoot() const
{
    const auto rows = tableView_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* p = vm_.pluginModel()->pluginAt(rows.first().row());
    if (!p) return {};
    if (!p->rootPath.empty()) return QString::fromStdString(p->rootPath);
    return infraVm_.defaultPluginsPath() + "/cosmos-" + QString::fromStdString(p->name);
}

QString PluginView::selectedComponentPath() const
{
    const auto* item = componentList_->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

} // namespace OpenC3::UI::Views
