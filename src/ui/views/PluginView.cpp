#include "PluginView.h"
#include "ui/dialogs/AddTargetDialog.h"
#include "ui/dialogs/CmdTlmFieldDialog.h"
#include "ui/dialogs/PluginWizard.h"
#include "ui/widgets/CmdTlmHighlighter.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

namespace OpenC3::UI::Views {

namespace {

const QString kCmdTemplate =
    "COMMAND TARGET COMMAND_NAME BIG_ENDIAN \"Command description\"\n"
    "  APPEND_PARAMETER PARAM_NAME 16 UINT 0 65535 0 \"Parameter description\"\n";

const QString kTlmTemplate =
    "TELEMETRY TARGET PACKET_NAME BIG_ENDIAN \"Telemetry description\"\n"
    "  APPEND_ITEM ITEM_NAME 16 UINT \"Item description\"\n";

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

int componentCategory(const QString& path)
{
    if (path.contains("/cmd_tlm/", Qt::CaseInsensitive)) return 0;
    if (path.contains("/screens/", Qt::CaseInsensitive)) return 1;
    if (path.contains("/procedures/", Qt::CaseInsensitive)) return 2;
    if (path == "plugin.txt" || path.endsWith(".gemspec", Qt::CaseInsensitive)) return 3;
    return 4;
}

QString componentCategoryTitle(int category)
{
    switch (category) {
    case 0: return "CMD / TLM Definitions";
    case 1: return "Screens";
    case 2: return "Procedures";
    case 3: return "Plugin Metadata";
    default: return "Other Files";
    }
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

QString quotedValue(QString value)
{
    value.replace('"', '\'');
    return '"' + value + '"';
}

// Pull the produced gem filename out of a `gem build` log. The build command
// appends the `.gem` path, and gem itself prints a "File: <name>.gem" line.
QString extractGemPath(const QString& buildOutput)
{
    const QStringList lines = buildOutput.split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString line = it->trimmed();
        if (line.endsWith(".gem", Qt::CaseInsensitive)) {
            const int fileIdx = line.indexOf("File:", 0, Qt::CaseInsensitive);
            return fileIdx >= 0 ? line.mid(fileIdx + 5).trimmed() : line;
        }
    }
    return {};
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
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(12);
    auto* titleBlock = new QVBoxLayout;
    titleBlock->setSpacing(2);
    auto* title = new QLabel("Plugin Manager", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setObjectName("PageTitle");
    auto* subtitle = new QLabel("플러그인을 선택하고 CMD/TLM 파일을 열어 필드를 편집·검증한 뒤 빌드하세요.", this);
    subtitle->setObjectName("SubLabel");
    subtitle->setWordWrap(true);
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    headerRow->addLayout(titleBlock, 1);
    root->addLayout(headerRow);

    auto* toolbarBlock = new QHBoxLayout;
    toolbarBlock->setSpacing(8);
    refreshBtn_ = new QPushButton("새로고침", this);
    installBtn_ = new QPushButton("Gem 설치", this);
    removeBtn_ = new QPushButton("삭제", this);
    validateBtn_ = new QPushButton("검증", this);
    buildBtn_ = new QPushButton("빌드", this);
    scaffoldBtn_ = new QPushButton("새 플러그인", this);
    addTargetBtn_ = new QPushButton("타겟 추가", this);

    scaffoldBtn_->setToolTip("새 OpenC3 플러그인 폴더 구조를 생성합니다.");
    addTargetBtn_->setToolTip("선택한 플러그인에 타겟 폴더 구조를 추가합니다.");

    removeBtn_->setEnabled(false);
    buildBtn_->setEnabled(false);
    addTargetBtn_->setEnabled(false);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    progressBar_->setFixedWidth(120);

    buildBtn_->setObjectName("PrimaryButton");

    const QList<QPushButton*> topButtons = {
        refreshBtn_, scaffoldBtn_, addTargetBtn_, validateBtn_,
        buildBtn_, installBtn_, removeBtn_
    };
    for (auto* button : topButtons) {
        button->setMinimumWidth(96);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    toolbarBlock->addWidget(refreshBtn_);
    toolbarBlock->addSpacing(8);
    toolbarBlock->addWidget(scaffoldBtn_);
    toolbarBlock->addWidget(addTargetBtn_);
    toolbarBlock->addSpacing(8);
    toolbarBlock->addWidget(validateBtn_);
    toolbarBlock->addWidget(buildBtn_);
    toolbarBlock->addWidget(installBtn_);
    toolbarBlock->addWidget(removeBtn_);
    toolbarBlock->addStretch();
    toolbarBlock->addWidget(progressBar_);
    root->addLayout(toolbarBlock);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    pluginSummaryLabel_ = new QLabel("Select a plugin folder to inspect and edit its files.", this);
    pluginSummaryLabel_->setObjectName("SubLabel");
    pluginSummaryLabel_->setWordWrap(true);
    root->addWidget(pluginSummaryLabel_);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setObjectName("PluginWorkbench");

    auto* leftPane = new QWidget(mainSplitter);
    leftPane->setObjectName("PluginLeftPane");
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto* pluginListGroup = new QGroupBox("플러그인 폴더", leftPane);
    auto* pluginListLayout = new QVBoxLayout(pluginListGroup);
    pluginListLayout->setContentsMargins(8, 14, 8, 8);
    tableView_ = new QTableView(pluginListGroup);
    tableView_->setModel(vm_.pluginModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setShowGrid(false);
    tableView_->setSortingEnabled(true);
    tableView_->setMinimumHeight(150);
    tableView_->setMaximumHeight(230);
    tableView_->setColumnHidden(ViewModels::PluginTableModel::Version, true);
    tableView_->horizontalHeader()->setStretchLastSection(false);
    tableView_->horizontalHeader()->setSectionResizeMode(ViewModels::PluginTableModel::Name, QHeaderView::Stretch);
    tableView_->horizontalHeader()->setSectionResizeMode(ViewModels::PluginTableModel::Status, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(ViewModels::PluginTableModel::Author, QHeaderView::ResizeToContents);
    tableView_->setColumnWidth(ViewModels::PluginTableModel::Status, 96);
    tableView_->setColumnWidth(ViewModels::PluginTableModel::Author, 58);
    pluginListLayout->addWidget(tableView_);
    leftLayout->addWidget(pluginListGroup, 0);

    auto* componentListGroup = new QGroupBox("플러그인 파일", leftPane);
    auto* componentListLayout = new QVBoxLayout(componentListGroup);
    componentListLayout->setContentsMargins(8, 14, 8, 8);
    componentListLayout->setSpacing(6);
    componentHintLabel_ = new QLabel("Select a plugin first.", componentListGroup);
    componentHintLabel_->setObjectName("SubLabel");
    componentHintLabel_->setWordWrap(true);
    componentList_ = new QListWidget(componentListGroup);
    componentList_->setObjectName("PluginComponentList");
    componentList_->setMinimumWidth(280);
    componentListLayout->addWidget(componentHintLabel_);
    componentListLayout->addWidget(componentList_, 1);
    leftLayout->addWidget(componentListGroup, 1);
    leftPane->setMinimumWidth(330);

    detailTabs_ = new QTabWidget(mainSplitter);
    detailTabs_->setObjectName("PluginDetailTabs");

    auto* detailTab = new QWidget(detailTabs_);
    auto* detailLayout = new QVBoxLayout(detailTab);
    detailLayout->setContentsMargins(8, 8, 8, 8);
    detailEdit_ = new QTextEdit(detailTab);
    detailEdit_->setReadOnly(true);
    detailEdit_->setObjectName("LogArea");
    detailLayout->addWidget(detailEdit_);
    detailTabs_->addTab(detailTab, "개요");

    auto* componentTab = new QWidget(detailTabs_);
    auto* componentLayout = new QVBoxLayout(componentTab);
    componentLayout->setContentsMargins(8, 8, 8, 8);
    componentLayout->setSpacing(8);

    auto* componentSplitter = new QSplitter(Qt::Horizontal, componentTab);

    auto* editorPane = new QWidget(componentSplitter);
    auto* editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    auto* componentToolbarBlock = new QVBoxLayout;
    componentToolbarBlock->setSpacing(6);
    auto* componentPathRow = new QHBoxLayout;
    auto* componentFileRow = new QHBoxLayout;
    auto* componentEditRow = new QHBoxLayout;
    componentPathLabel_ = new QLabel("Select a plugin file", editorPane);
    componentPathLabel_->setObjectName("PluginFilePath");
    openComponentBtn_ = new QPushButton("파일 열기", editorPane);
    saveComponentBtn_ = new QPushButton("저장", editorPane);
    validateComponentBtn_ = new QPushButton("검증", editorPane);
    openInCmdTlmBtn_ = new QPushButton("CMD/TLM 뷰로", editorPane);
    startCmdTlmEditBtn_ = new QPushButton("CMD/TLM 편집 시작", editorPane);
    insertCmdBtn_ = new QPushButton("+ COMMAND", editorPane);
    insertTlmBtn_ = new QPushButton("+ TELEMETRY", editorPane);
    addFieldBtn_ = new QPushButton("필드 추가", editorPane);
    addStructureFieldBtn_ = new QPushButton("행 추가", editorPane);
    deleteStructureFieldBtn_ = new QPushButton("행 삭제", editorPane);
    refreshStructureBtn_ = new QPushButton("구조 새로고침", editorPane);
    applyStructureBtn_ = new QPushButton("선택 필드 적용", editorPane);
    toggleReferenceBtn_ = new QPushButton("참고 표시", editorPane);
    toggleReferenceBtn_->setCheckable(true);
    toggleReferenceBtn_->setMinimumWidth(120);
    toggleReferenceBtn_->setToolTip("Show or hide the CMD/TLM quick reference panel.");
    startCmdTlmEditBtn_->setObjectName("PrimaryButton");
    saveComponentBtn_->setObjectName("PrimaryButton");
    openComponentBtn_->setEnabled(false);
    saveComponentBtn_->setEnabled(false);
    validateComponentBtn_->setEnabled(false);
    openInCmdTlmBtn_->setEnabled(false);
    startCmdTlmEditBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);
    addStructureFieldBtn_->setEnabled(false);
    deleteStructureFieldBtn_->setEnabled(false);
    refreshStructureBtn_->setEnabled(false);
    applyStructureBtn_->setEnabled(false);
    openComponentBtn_->setMinimumWidth(90);
    saveComponentBtn_->setMinimumWidth(80);
    validateComponentBtn_->setMinimumWidth(130);
    openInCmdTlmBtn_->setMinimumWidth(130);
    startCmdTlmEditBtn_->setMinimumWidth(150);
    insertCmdBtn_->setMinimumWidth(110);
    insertTlmBtn_->setMinimumWidth(120);
    addFieldBtn_->setMinimumWidth(110);
    addStructureFieldBtn_->setMinimumWidth(90);
    deleteStructureFieldBtn_->setMinimumWidth(95);
    refreshStructureBtn_->setMinimumWidth(130);
    applyStructureBtn_->setMinimumWidth(150);

    componentPathRow->addWidget(componentPathLabel_, 1);
    componentPathRow->addWidget(startCmdTlmEditBtn_);
    componentFileRow->addWidget(openComponentBtn_);
    componentFileRow->addWidget(addFieldBtn_);
    componentFileRow->addWidget(addStructureFieldBtn_);
    componentFileRow->addWidget(deleteStructureFieldBtn_);
    componentFileRow->addWidget(validateComponentBtn_);
    componentFileRow->addWidget(applyStructureBtn_);
    componentFileRow->addStretch();
    componentFileRow->addWidget(saveComponentBtn_);
    componentEditRow->addWidget(insertCmdBtn_);
    componentEditRow->addWidget(insertTlmBtn_);
    componentEditRow->addWidget(refreshStructureBtn_);
    componentEditRow->addWidget(openInCmdTlmBtn_);
    componentEditRow->addStretch();
    componentEditRow->addWidget(toggleReferenceBtn_);
    componentToolbarBlock->addLayout(componentPathRow);
    componentToolbarBlock->addLayout(componentFileRow);
    componentToolbarBlock->addLayout(componentEditRow);
    editorLayout->addLayout(componentToolbarBlock);

    componentEditor_ = new QTextEdit(editorPane);
    componentEditor_->setObjectName("CodeEditor");
    componentEditor_->setLineWrapMode(QTextEdit::NoWrap);
    componentEditor_->setPlaceholderText("Open a plugin file to edit it here.");
    componentEditor_->setMinimumHeight(220);
    highlighter_ = new Widgets::CmdTlmHighlighter(componentEditor_->document());

    auto* structureGroup = new QGroupBox("구조 편집기 (Block / Field)", editorPane);
    auto* structureLayout = new QVBoxLayout(structureGroup);

    // ── Block editor (COMMAND / TELEMETRY header) ────────────────────────────
    // Lets the user retarget / rename / re-describe a whole packet without
    // touching the raw COMMAND/TELEMETRY line by hand.
    auto* blockSelectRow = new QHBoxLayout;
    blockSelectRow->setSpacing(6);
    blockSelectRow->addWidget(new QLabel("Block:", structureGroup));
    blockSelectorCombo_ = new QComboBox(structureGroup);
    blockSelectorCombo_->setMinimumWidth(220);
    blockSelectorCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    blockKindLabel_ = new QLabel("", structureGroup);
    blockKindLabel_->setObjectName("SubLabel");
    blockSelectRow->addWidget(blockSelectorCombo_, 1);
    blockSelectRow->addWidget(blockKindLabel_);
    structureLayout->addLayout(blockSelectRow);

    auto* blockFieldRow = new QHBoxLayout;
    blockFieldRow->setSpacing(6);
    blockTargetEdit_ = new QLineEdit(structureGroup);
    blockTargetEdit_->setPlaceholderText("TARGET");
    blockNameEdit_ = new QLineEdit(structureGroup);
    blockNameEdit_->setPlaceholderText("PACKET / COMMAND");
    blockEndiannessCombo_ = new QComboBox(structureGroup);
    blockEndiannessCombo_->addItems({"BIG_ENDIAN", "LITTLE_ENDIAN"});
    blockDescriptionEdit_ = new QLineEdit(structureGroup);
    blockDescriptionEdit_->setPlaceholderText("Description");
    applyBlockBtn_ = new QPushButton("블록 적용", structureGroup);
    applyBlockBtn_->setMinimumWidth(96);
    applyBlockBtn_->setToolTip("선택한 COMMAND/TELEMETRY 라인을 갱신합니다. "
                              "주석과 하위 필드는 그대로 유지됩니다.");
    blockFieldRow->addWidget(new QLabel("Target:", structureGroup));
    blockFieldRow->addWidget(blockTargetEdit_);
    blockFieldRow->addWidget(new QLabel("Name:", structureGroup));
    blockFieldRow->addWidget(blockNameEdit_);
    blockFieldRow->addWidget(new QLabel("Endian:", structureGroup));
    blockFieldRow->addWidget(blockEndiannessCombo_);
    blockFieldRow->addWidget(new QLabel("Desc:", structureGroup));
    blockFieldRow->addWidget(blockDescriptionEdit_, 1);
    blockFieldRow->addWidget(applyBlockBtn_);
    structureLayout->addLayout(blockFieldRow);

    blockSelectorCombo_->setEnabled(false);
    blockTargetEdit_->setEnabled(false);
    blockNameEdit_->setEnabled(false);
    blockEndiannessCombo_->setEnabled(false);
    blockDescriptionEdit_->setEnabled(false);
    applyBlockBtn_->setEnabled(false);

    structureTable_ = new QTableWidget(0, 9, structureGroup);
    structureTable_->setObjectName("PluginStructureTable");
    structureTable_->setHorizontalHeaderLabels({
        "Line", "Block", "Field", "Bits", "Type",
        "Min", "Max", "Default", "Description"
    });
    structureTable_->horizontalHeader()->setStretchLastSection(true);
    structureTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    structureTable_->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    structureTable_->verticalHeader()->setVisible(false);
    structureTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    structureTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    structureTable_->setMinimumHeight(140);
    structureTable_->setMaximumHeight(240);
    structureTable_->setColumnWidth(0, 48);   // Line
    structureLayout->addWidget(structureTable_, 1);

    componentDiagnostics_ = new QTextEdit(editorPane);
    componentDiagnostics_->setObjectName("LogArea");
    componentDiagnostics_->setReadOnly(true);
    componentDiagnostics_->setMaximumHeight(92);

    auto* editorVerticalSplitter = new QSplitter(Qt::Vertical, editorPane);
    editorVerticalSplitter->addWidget(structureGroup);
    editorVerticalSplitter->addWidget(componentEditor_);
    editorVerticalSplitter->addWidget(componentDiagnostics_);
    editorVerticalSplitter->setChildrenCollapsible(false);
    editorVerticalSplitter->setStretchFactor(0, 0);
    editorVerticalSplitter->setStretchFactor(1, 1);
    editorVerticalSplitter->setStretchFactor(2, 0);
    editorVerticalSplitter->setSizes({300, 330, 84});
    editorLayout->addWidget(editorVerticalSplitter, 1);

    guideGroup_ = new QGroupBox("참고 (Quick Reference)", componentSplitter);
    auto* guideLayout = new QVBoxLayout(guideGroup_);
    componentGuide_ = new QTextEdit(guideGroup_);
    componentGuide_->setObjectName("LogArea");
    componentGuide_->setReadOnly(true);
    componentGuide_->setLineWrapMode(QTextEdit::WidgetWidth);
    componentGuide_->setPlainText(syntaxGuideText());
    guideLayout->addWidget(componentGuide_);

    componentSplitter->addWidget(editorPane);
    componentSplitter->addWidget(guideGroup_);
    componentSplitter->setChildrenCollapsible(false); // panes must not vanish on drag
    componentSplitter->setStretchFactor(0, 1);
    componentSplitter->setStretchFactor(1, 0);
    componentSplitter->setSizes({820, 260});
    editorPane->setMinimumWidth(520);
    guideGroup_->setMinimumWidth(220);
    guideGroup_->setMaximumWidth(340);
    guideGroup_->setVisible(false); // collapsed by default; editor gets the space
    componentLayout->addWidget(componentSplitter, 1);
    detailTabs_->addTab(componentTab, "파일 편집");

    mainSplitter->addWidget(leftPane);
    mainSplitter->addWidget(detailTabs_);
    mainSplitter->setChildrenCollapsible(false); // table and detail stay visible
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({360, 980});
    root->addWidget(mainSplitter, 1);
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
    connect(startCmdTlmEditBtn_, &QPushButton::clicked, this, &PluginView::onStartCmdTlmEditClicked);
    connect(insertCmdBtn_, &QPushButton::clicked, this, &PluginView::onInsertCmdClicked);
    connect(insertTlmBtn_, &QPushButton::clicked, this, &PluginView::onInsertTlmClicked);
    connect(addFieldBtn_, &QPushButton::clicked, this, &PluginView::onAddFieldClicked);
    connect(addStructureFieldBtn_, &QPushButton::clicked, this, &PluginView::onAddStructureFieldClicked);
    connect(deleteStructureFieldBtn_, &QPushButton::clicked, this, &PluginView::onDeleteStructureFieldClicked);
    connect(refreshStructureBtn_, &QPushButton::clicked, this, &PluginView::onRefreshStructureClicked);
    connect(applyStructureBtn_, &QPushButton::clicked, this, &PluginView::onApplyStructureClicked);
    connect(applyBlockBtn_, &QPushButton::clicked, this, &PluginView::onApplyBlockClicked);
    connect(blockSelectorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PluginView::onBlockSelectionChanged);
    connect(toggleReferenceBtn_, &QPushButton::clicked, this, &PluginView::onToggleReferenceClicked);
    connect(componentEditor_, &QTextEdit::textChanged,
            this, [this] {
                if (!currentComponentPath_.isEmpty())
                    setComponentDirty(true);
            });
    connect(structureTable_, &QTableWidget::itemSelectionChanged,
            this, [this] {
                applyStructureBtn_->setEnabled(isCmdTlmFile(currentComponentPath_)
                    && !structureTable_->selectedItems().isEmpty());
                deleteStructureFieldBtn_->setEnabled(isCmdTlmFile(currentComponentPath_)
                    && !structureTable_->selectedItems().isEmpty());
                const auto selected = structureTable_->selectionModel()->selectedRows();
                if (!selected.isEmpty())
                    focusEditorLineForStructureRow(selected.first().row());
            });
    connect(structureTable_, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) {
                applyStructureRowToEditor(row);
            });

    connect(componentList_, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item) {
                if (!item) return;
                if (!(item->flags() & Qt::ItemIsSelectable)) return;
                componentPathLabel_->setText("Selected: " + item->data(Qt::UserRole + 1).toString());
                openComponentBtn_->setEnabled(true);
                setComponentHint(isCmdTlmFile(item->data(Qt::UserRole).toString())
                    ? "Open this CMD/TLM file to edit definitions, add fields, validate, and save."
                    : "Open this plugin file to inspect or edit its text.");
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
                saveComponentBtn_->setEnabled(!busy && componentDirty_);
                openComponentBtn_->setEnabled(!busy && componentList_->currentItem() != nullptr);
                validateComponentBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                openInCmdTlmBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                startCmdTlmEditBtn_->setEnabled(!busy && !firstCmdTlmComponentPath_.isEmpty());
                addFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                addStructureFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                deleteStructureFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_)
                    && !structureTable_->selectedItems().isEmpty());
                refreshStructureBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
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
                if (pendingBuild_) {
                    // Build result: surface the gem path / failure log clearly.
                    // The popup is shown from actionCompleted to avoid duplicates.
                    if (valid) {
                        const QString gem = extractGemPath(summary);
                        detailEdit_->setPlainText(
                            "[빌드 성공]\n생성된 gem 파일: "
                            + (gem.isEmpty() ? "(경로를 확인하지 못했습니다)" : gem)
                            + "\n\n--- 빌드 로그 ---\n" + summary);
                    } else {
                        detailEdit_->setPlainText(
                            "[빌드 실패] 아래 로그에서 원인을 확인하세요.\n\n"
                            "--- 빌드 로그 ---\n" + summary);
                    }
                    return;
                }
                detailEdit_->setPlainText(summary);
                if (!valid)
                    QMessageBox::warning(this, "Plugin Manager", summary);
            });

    connect(&vm_, &ViewModels::PluginViewModel::actionCompleted,
            this, [this](const QString& name, bool ok) {
                if (pendingBuild_) {
                    pendingBuild_ = false;
                    if (ok)
                        QMessageBox::information(this, "플러그인 빌드",
                            "빌드가 완료되었습니다.\nOverview 탭에서 생성된 gem 경로를 확인하세요.");
                    else
                        QMessageBox::warning(this, "플러그인 빌드",
                            "빌드에 실패했습니다.\nOverview 탭의 빌드 로그에서 원인을 확인하세요.");
                    return;
                }
                if (!ok)
                    QMessageBox::warning(this, "Plugin Manager",
                        "Action failed for: " + name);
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::pluginFilesListed,
            this, &PluginView::populateComponentList);

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileOpened,
            this, [this](const QString& path, const QString& content) {
                if (currentPluginRoot_.isEmpty()
                    || !path.startsWith(currentPluginRoot_ + "/", Qt::CaseInsensitive))
                    return;

                currentComponentPath_ = path;
                currentComponentDisplayPath_ = path;
                if (currentComponentDisplayPath_.startsWith(currentPluginRoot_ + "/", Qt::CaseInsensitive))
                    currentComponentDisplayPath_ = currentComponentDisplayPath_.mid(currentPluginRoot_.size() + 1);
                componentPathLabel_->setToolTip(path);
                componentEditor_->setPlainText(content);
                setComponentDirty(false);
                const bool cmdTlm = isCmdTlmFile(path);
                validateComponentBtn_->setEnabled(cmdTlm);
                openInCmdTlmBtn_->setEnabled(cmdTlm);
                insertCmdBtn_->setEnabled(cmdTlm);
                insertTlmBtn_->setEnabled(cmdTlm);
                addFieldBtn_->setEnabled(cmdTlm);
                addStructureFieldBtn_->setEnabled(cmdTlm);
                deleteStructureFieldBtn_->setEnabled(false);
                refreshStructureBtn_->setEnabled(cmdTlm);
                applyStructureBtn_->setEnabled(false);
                setComponentHint(cmdTlm
                    ? "Editing CMD/TLM. Add fields with the form, validate before saving, then build the plugin."
                    : "Editing a plugin text file. Review changes carefully before saving.");
                componentDiagnostics_->setPlainText(
                    cmdTlm ? "Loaded CMD/TLM definition." : "Loaded text file.");
                refreshStructureTable();
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileSaved,
            this, [this](const QString& path, bool success) {
                if (path != currentComponentPath_)
                    return;

                if (success)
                    setComponentDirty(false);
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
    if (componentDirty_) {
        QMessageBox::information(this, "Build Plugin",
            "Save the open CMD/TLM file before building the plugin.");
        return;
    }

    const QString root = selectedPluginRoot();
    if (root.isEmpty()) {
        QMessageBox::information(this, "Build Plugin",
            "Select a plugin folder first.");
        return;
    }
    pendingBuild_ = true;
    detailTabs_->setCurrentIndex(0);
    detailEdit_->setPlainText("플러그인 빌드 중...\n" + root);
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
    structureTable_->setRowCount(0);
    currentBlocks_.clear();
    refreshBlockEditor();
    currentPluginRoot_.clear();
    currentComponentPath_.clear();
    currentComponentDisplayPath_.clear();
    firstCmdTlmComponentPath_.clear();
    componentDirty_ = false;
    componentPathLabel_->setText(hasSel ? "Loading plugin files" : "Select a plugin file");
    setComponentHint(hasSel
        ? "Loading plugin files. CMD/TLM files will appear grouped first."
        : "Select a plugin first.");
    saveComponentBtn_->setEnabled(false);
    openComponentBtn_->setEnabled(false);
    validateComponentBtn_->setEnabled(false);
    openInCmdTlmBtn_->setEnabled(false);
    startCmdTlmEditBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);
    addStructureFieldBtn_->setEnabled(false);
    deleteStructureFieldBtn_->setEnabled(false);
    refreshStructureBtn_->setEnabled(false);
    applyStructureBtn_->setEnabled(false);

    if (!hasSel) {
        detailEdit_->clear();
        pluginSummaryLabel_->setText("Select a plugin folder to inspect and edit its files.");
        setComponentHint("Select a plugin first.");
        return;
    }

    const auto* plugin =
        vm_.pluginModel()->pluginAt(tableView_->selectionModel()->selectedRows().first().row());
    if (!plugin) return;

    const QString pluginName = QString::fromStdString(plugin->name);
    const QString pluginRoot = QString::fromStdString(plugin->rootPath);
    const int targetCount = static_cast<int>(plugin->targets.size());
    pluginSummaryLabel_->setText(QString("%1  |  %2 target(s)  |  %3")
        .arg(pluginName)
        .arg(targetCount)
        .arg(pluginRoot));

    const QString detail = QString(
        "Selected Plugin\n"
        "Name:       %1\n"
        "Root:       %2\n"
        "Status:     %3\n\n"
        "Build Inputs\n"
        "plugin.txt: %4\n"
        "gemspec:    %5\n"
        "gem:        %6\n\n"
        "Metadata\n"
        "Version:    %7\n"
        "Author:     %8\n"
        "COSMOS:     %9\n\n"
        "%10\n\n"
        "Targets:\n%11\n\n"
        "Next Steps\n"
        "1. Open the Edit Files tab.\n"
        "2. Open a CMD/TLM file from the grouped list.\n"
        "3. Use Add Field, Validate CMD/TLM, then Save.")
        .arg(QString::fromStdString(plugin->name))
        .arg(QString::fromStdString(plugin->rootPath))
        .arg(plugin->isInstalled() ? "Ready" : "Needs attention")
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
    if (!confirmSaveAfterValidation())
        return;
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
    refreshStructureTable();
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

void PluginView::onStartCmdTlmEditClicked()
{
    if (firstCmdTlmComponentPath_.isEmpty()) {
        componentDiagnostics_->setPlainText("No CMD/TLM definition file was found in this plugin.");
        return;
    }
    if (!confirmDiscardUnsavedChanges())
        return;

    detailTabs_->setCurrentIndex(1);
    cmdTlmVm_.openFile(firstCmdTlmComponentPath_);
}

void PluginView::onInsertCmdClicked()
{
    insertTextAtCursor(kCmdTemplate);
    refreshStructureTable();
}

void PluginView::onInsertTlmClicked()
{
    insertTextAtCursor(kTlmTemplate);
    refreshStructureTable();
}

void PluginView::onAddFieldClicked()
{
    Dialogs::CmdTlmFieldDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        insertTextAtCursor(dialog.generatedLine());
        refreshStructureTable();
    }
}

void PluginView::onAddStructureFieldClicked()
{
    if (!isCmdTlmFile(currentComponentPath_))
        return;

    Dialogs::CmdTlmFieldDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto selected = structureTable_->selectionModel()->selectedRows();
    const int row = selected.isEmpty() ? -1 : selected.first().row();
    insertStructureFieldAfterRow(row, dialog.generatedLine());
}

void PluginView::onDeleteStructureFieldClicked()
{
    const auto selected = structureTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        componentDiagnostics_->setPlainText("Select a field row first.");
        return;
    }

    const auto* lineCell = structureTable_->item(selected.first().row(), 0);
    if (!lineCell)
        return;

    const int lineNumber = lineCell->data(Qt::UserRole).toInt();
    const QString fieldName = structureTable_->item(selected.first().row(), 2)
        ? structureTable_->item(selected.first().row(), 2)->text()
        : QString("field");
    const auto answer = QMessageBox::question(
        this,
        "Delete Field",
        QString("Delete field '%1' from this CMD/TLM file?").arg(fieldName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    deleteEditorLine(lineNumber);
    refreshStructureTable();
    componentDiagnostics_->setPlainText(QString("Deleted field '%1'. Save the file to keep it.")
        .arg(fieldName));
}

void PluginView::onRefreshStructureClicked()
{
    refreshStructureTable();
}

void PluginView::onApplyStructureClicked()
{
    const auto selected = structureTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        componentDiagnostics_->setPlainText("Select a field row first.");
        return;
    }
    applyStructureRowToEditor(selected.first().row());
}

void PluginView::populateComponentList(const QStringList& files, const QString& pluginRootPath)
{
    if (pluginRootPath != currentPluginRoot_)
        return;

    componentList_->clear();
    QStringList sortedFiles = files;
    sortedFiles.sort(Qt::CaseInsensitive);

    int currentCategory = -1;
    int cmdTlmCount = 0;
    firstCmdTlmComponentPath_.clear();
    for (const QString& relPath : sortedFiles) {
        if (isCmdTlmFile(relPath)) {
            ++cmdTlmCount;
            if (firstCmdTlmComponentPath_.isEmpty())
                firstCmdTlmComponentPath_ = pluginRootPath + "/" + relPath;
        }
    }

    for (const QString& relPath : sortedFiles) {
        const int category = componentCategory(relPath);
        if (category != currentCategory) {
            currentCategory = category;
            auto* header = new QListWidgetItem(componentCategoryTitle(category));
            QFont headerFont = header->font();
            headerFont.setBold(true);
            header->setFont(headerFont);
            header->setForeground(QColor("#9CDCFE"));
            header->setFlags(Qt::NoItemFlags);
            componentList_->addItem(header);
        }

        QString displayPath = relPath;
        displayPath.replace("targets/", "");
        const QString kind = componentKind(relPath);
        auto* item = new QListWidgetItem(QString("  %1  %2").arg(kind, displayPath));
        item->setData(Qt::UserRole, pluginRootPath + "/" + relPath);
        item->setData(Qt::UserRole + 1, relPath);
        item->setToolTip(relPath);
        if (isCmdTlmFile(relPath))
            item->setForeground(QColor("#4EC994"));
        componentList_->addItem(item);
    }

    componentPathLabel_->setText(files.isEmpty()
        ? "No plugin component files found"
        : "Select a plugin file");
    setComponentHint(files.isEmpty()
        ? "No editable plugin files were found. Check that plugin.txt, gemspec, and targets exist."
        : QString("Found %1 file(s), including %2 CMD/TLM definition file(s). Use Start CMD/TLM Edit for the usual edit flow.")
              .arg(files.size()).arg(cmdTlmCount));
    startCmdTlmEditBtn_->setEnabled(cmdTlmCount > 0 && !cmdTlmVm_.isBusy());
}

void PluginView::setComponentHint(const QString& text)
{
    if (componentHintLabel_)
        componentHintLabel_->setText(text);
}

void PluginView::openSelectedComponent(QListWidgetItem* item)
{
    if (!item) return;
    if (!(item->flags() & Qt::ItemIsSelectable)) return;
    if (!confirmDiscardUnsavedChanges())
        return;
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

void PluginView::refreshStructureTable()
{
    structureTable_->setRowCount(0);
    applyStructureBtn_->setEnabled(false);

    if (!isCmdTlmFile(currentComponentPath_)) {
        currentBlocks_.clear();
        refreshBlockEditor();
        return;
    }

    const auto result = ViewModels::CmdTlmParser::parse(componentEditor_->toPlainText());
    currentBlocks_ = result.blocks;
    refreshBlockEditor();
    int row = 0;
    for (const auto& block : result.blocks) {
        const bool isCommand = block.kind == ViewModels::CmdTlmBlock::Kind::Command;
        const QString blockLabel = QString("%1 %2::%3")
            .arg(isCommand ? "COMMAND" : "TELEMETRY", block.targetName, block.name);

        for (const auto& item : block.items) {
            structureTable_->insertRow(row);

            auto* lineItem = new QTableWidgetItem(QString::number(item.lineNumber));
            lineItem->setFlags(lineItem->flags() & ~Qt::ItemIsEditable);
            lineItem->setData(Qt::UserRole, item.lineNumber);
            lineItem->setData(Qt::UserRole + 1, isCommand);
            lineItem->setData(Qt::UserRole + 2, item.isId);

            auto* blockItem = new QTableWidgetItem(blockLabel);
            blockItem->setFlags(blockItem->flags() & ~Qt::ItemIsEditable);

            structureTable_->setItem(row, 0, lineItem);
            structureTable_->setItem(row, 1, blockItem);
            structureTable_->setItem(row, 2, new QTableWidgetItem(item.name));
            structureTable_->setItem(row, 3, new QTableWidgetItem(QString::number(item.bitSize)));
            structureTable_->setItem(row, 4, new QTableWidgetItem(item.dataType));
            structureTable_->setItem(row, 5, new QTableWidgetItem(item.minVal));
            structureTable_->setItem(row, 6, new QTableWidgetItem(item.maxVal));
            structureTable_->setItem(row, 7, new QTableWidgetItem(item.defaultVal));
            structureTable_->setItem(row, 8, new QTableWidgetItem(item.description));
            ++row;
        }
    }

    componentDiagnostics_->setPlainText(QString(
        "Structure loaded: %1 block(s), %2 field(s), %3 error(s), %4 warning(s).")
        .arg(result.blocks.size())
        .arg(structureTable_->rowCount())
        .arg(result.errorCount())
        .arg(result.warningCount()));
}

void PluginView::refreshBlockEditor()
{
    const bool editable = isCmdTlmFile(currentComponentPath_) && !currentBlocks_.isEmpty();

    QSignalBlocker blocker(blockSelectorCombo_);
    blockSelectorCombo_->clear();
    for (const auto& block : currentBlocks_) {
        const bool isCommand = block.kind == ViewModels::CmdTlmBlock::Kind::Command;
        blockSelectorCombo_->addItem(QString("%1  %2::%3  (line %4)")
            .arg(isCommand ? "COMMAND" : "TELEMETRY",
                 block.targetName, block.name)
            .arg(block.lineNumber));
    }

    blockSelectorCombo_->setEnabled(editable);
    blockTargetEdit_->setEnabled(editable);
    blockNameEdit_->setEnabled(editable);
    blockEndiannessCombo_->setEnabled(editable);
    blockDescriptionEdit_->setEnabled(editable);
    applyBlockBtn_->setEnabled(editable);

    if (editable) {
        blockSelectorCombo_->setCurrentIndex(0);
        populateBlockForm(0);
    } else {
        blockKindLabel_->clear();
        blockTargetEdit_->clear();
        blockNameEdit_->clear();
        blockDescriptionEdit_->clear();
    }
}

void PluginView::populateBlockForm(int blockIndex)
{
    if (blockIndex < 0 || blockIndex >= currentBlocks_.size())
        return;

    const auto& block = currentBlocks_[blockIndex];
    const bool isCommand = block.kind == ViewModels::CmdTlmBlock::Kind::Command;
    blockKindLabel_->setText(isCommand ? "COMMAND" : "TELEMETRY");
    blockTargetEdit_->setText(block.targetName);
    blockNameEdit_->setText(block.name);
    blockDescriptionEdit_->setText(block.description);

    const QString endian = block.endianness.isEmpty() ? "BIG_ENDIAN" : block.endianness;
    const int endianIdx = blockEndiannessCombo_->findText(endian);
    blockEndiannessCombo_->setCurrentIndex(endianIdx >= 0 ? endianIdx : 0);
}

void PluginView::onBlockSelectionChanged(int index)
{
    populateBlockForm(index);
    const auto selected = (index >= 0 && index < currentBlocks_.size())
        ? currentBlocks_[index].lineNumber : 0;
    if (selected > 0) {
        QTextBlock block = componentEditor_->document()->findBlockByNumber(selected - 1);
        if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.select(QTextCursor::LineUnderCursor);
            componentEditor_->setTextCursor(cursor);
            componentEditor_->ensureCursorVisible();
        }
    }
}

void PluginView::onApplyBlockClicked()
{
    const int index = blockSelectorCombo_->currentIndex();
    if (index < 0 || index >= currentBlocks_.size()) {
        componentDiagnostics_->setPlainText("Select a COMMAND/TELEMETRY block first.");
        return;
    }

    const auto& block = currentBlocks_[index];
    const bool isCommand = block.kind == ViewModels::CmdTlmBlock::Kind::Command;

    const QString target = blockTargetEdit_->text().trimmed().toUpper();
    const QString name   = blockNameEdit_->text().trimmed().toUpper();
    const QString endian = blockEndiannessCombo_->currentText();
    const QString desc   = blockDescriptionEdit_->text().trimmed();

    if (target.isEmpty() || name.isEmpty()) {
        componentDiagnostics_->setPlainText("Target and Name are required for a block.");
        return;
    }

    QString line = QString("%1 %2 %3 %4")
        .arg(isCommand ? "COMMAND" : "TELEMETRY", target, name, endian);
    if (!desc.isEmpty())
        line += " " + quotedValue(desc);

    replaceEditorLine(block.lineNumber, line);
    componentDiagnostics_->setPlainText(QString(
        "Updated %1 block at line %2. Comments and fields were preserved. Save to keep it.")
        .arg(isCommand ? "COMMAND" : "TELEMETRY")
        .arg(block.lineNumber));
    refreshStructureTable();
}

void PluginView::onToggleReferenceClicked()
{
    const bool show = toggleReferenceBtn_->isChecked();
    guideGroup_->setVisible(show);
    toggleReferenceBtn_->setText(show ? "참고 숨기기" : "참고 표시");
}

void PluginView::applyStructureRowToEditor(int row)
{
    if (row < 0 || row >= structureTable_->rowCount())
        return;

    auto* lineCell = structureTable_->item(row, 0);
    if (!lineCell)
        return;

    const int lineNumber = lineCell->data(Qt::UserRole).toInt();
    const bool isCommand = lineCell->data(Qt::UserRole + 1).toBool();
    const bool isId = lineCell->data(Qt::UserRole + 2).toBool();
    if (lineNumber <= 0)
        return;

    const auto textAt = [this, row](int col) {
        const auto* item = structureTable_->item(row, col);
        return item ? item->text().trimmed() : QString{};
    };

    const QString keyword = isCommand
        ? (isId ? "APPEND_ID_PARAMETER" : "APPEND_PARAMETER")
        : (isId ? "APPEND_ID_ITEM" : "APPEND_ITEM");
    const QString name = textAt(2).toUpper();
    const QString bits = textAt(3);
    const QString type = textAt(4).toUpper();
    const QString description = quotedValue(textAt(8));

    if (name.isEmpty() || bits.isEmpty() || type.isEmpty()) {
        componentDiagnostics_->setPlainText("Name, Bits, and Type are required.");
        return;
    }

    QString replacement;
    if (isCommand) {
        replacement = QString("  %1 %2 %3 %4 %5 %6 %7 %8")
            .arg(keyword, name, bits, type, textAt(5), textAt(6), textAt(7), description);
    } else {
        replacement = QString("  %1 %2 %3 %4 %5")
            .arg(keyword, name, bits, type, description);
    }

    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid()) {
        componentDiagnostics_->setPlainText("Could not find the selected line in the editor.");
        return;
    }

    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.insertText(replacement);
    componentEditor_->setTextCursor(cursor);
    componentDiagnostics_->setPlainText(QString("Applied field change to line %1. Save the file to keep it.")
        .arg(lineNumber));
    refreshStructureTable();
}

void PluginView::focusEditorLineForStructureRow(int row)
{
    if (row < 0 || row >= structureTable_->rowCount())
        return;

    const auto* lineCell = structureTable_->item(row, 0);
    if (!lineCell)
        return;

    const int lineNumber = lineCell->data(Qt::UserRole).toInt();
    if (lineNumber <= 0)
        return;

    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    componentEditor_->setTextCursor(cursor);
    componentEditor_->ensureCursorVisible();
}

void PluginView::insertStructureFieldAfterRow(int row, const QString& line)
{
    QString fieldLine = line;
    while (fieldLine.endsWith('\n') || fieldLine.endsWith('\r'))
        fieldLine.chop(1);

    QTextCursor cursor = componentEditor_->textCursor();
    if (row >= 0 && row < structureTable_->rowCount()) {
        const auto* lineCell = structureTable_->item(row, 0);
        const int lineNumber = lineCell ? lineCell->data(Qt::UserRole).toInt() : 0;
        QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
        if (block.isValid()) {
            cursor = QTextCursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock);
        }
    }

    cursor.insertText("\n" + fieldLine);
    componentEditor_->setTextCursor(cursor);
    refreshStructureTable();
    componentDiagnostics_->setPlainText("Added field row. Save the file to keep it.");
}

void PluginView::replaceEditorLine(int lineNumber, const QString& newText)
{
    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid()) {
        componentDiagnostics_->setPlainText("Could not find the selected line in the editor.");
        return;
    }

    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.insertText(newText);
    componentEditor_->setTextCursor(cursor);
    componentEditor_->ensureCursorVisible();
}

void PluginView::deleteEditorLine(int lineNumber)
{
    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.deleteChar();
}

void PluginView::setComponentDirty(bool dirty)
{
    if (componentDirty_ == dirty)
        return;

    componentDirty_ = dirty;
    saveComponentBtn_->setText(componentDirty_ ? "변경 저장" : "저장");
    saveComponentBtn_->setEnabled(componentDirty_ && !cmdTlmVm_.isBusy());
    updateComponentPathLabel();
}

void PluginView::updateComponentPathLabel()
{
    if (currentComponentDisplayPath_.isEmpty()) {
        componentPathLabel_->setText("Select a plugin file");
        return;
    }

    componentPathLabel_->setText(QString("%1  %2")
        .arg(componentDirty_ ? "저장 안 됨" : "편집 중", currentComponentDisplayPath_));
}

bool PluginView::confirmDiscardUnsavedChanges()
{
    if (!componentDirty_)
        return true;

    const auto answer = QMessageBox::question(
        this,
        "Unsaved Changes",
        "This CMD/TLM file has unsaved changes. Open another file and discard them?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return answer == QMessageBox::Yes;
}

bool PluginView::confirmSaveAfterValidation()
{
    if (!isCmdTlmFile(currentComponentPath_))
        return true;

    const auto result = ViewModels::CmdTlmParser::parse(componentEditor_->toPlainText());
    refreshStructureTable();
    if (result.errorCount() == 0)
        return true;

    QStringList lines;
    lines << QString("Validation found %1 error(s). Save anyway?")
                 .arg(result.errorCount());
    int shown = 0;
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.severity != ViewModels::CmdTlmDiagnostic::Severity::Error)
            continue;
        lines << QString("Line %1: %2").arg(diagnostic.line).arg(diagnostic.message);
        if (++shown >= 5)
            break;
    }

    componentDiagnostics_->setPlainText(lines.join('\n'));
    const auto answer = QMessageBox::warning(
        this,
        "Validate Before Save",
        lines.join('\n'),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return answer == QMessageBox::Yes;
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
