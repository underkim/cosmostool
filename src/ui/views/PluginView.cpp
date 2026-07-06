#include "PluginView.h"
#include "ui/dialogs/AddTargetDialog.h"
#include "ui/dialogs/NewScriptDialog.h"
#include "ui/dialogs/CmdTlmFieldDialog.h"
#include "ui/dialogs/PluginManifestInterfaceDialog.h"
#include "ui/dialogs/NewMicroserviceDialog.h"
#include "ui/dialogs/PluginManifestModifierDialog.h"
#include "ui/dialogs/ScreenWidgetDialog.h"
#include "ui/dialogs/PluginWizard.h"
#include "ui/views/LogViewerView.h"
#include "ui/widgets/CmdTlmHighlighter.h"
#include "ui/widgets/CmdTlmSnippets.h"
#include "ui/widgets/PluginManifestSnippets.h"
#include "viewmodels/validation/PluginKeywords.h"
#include "viewmodels/screen/ScreenLayoutParser.h"
#include "core/connection/ShellQuote.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QMenu>
#include <QVBoxLayout>

namespace OpenC3::UI::Views {

namespace {

// CMD/TLM insert templates and the cmd_tlm file-kind check
// (ui/widgets/CmdTlmSnippets.h).
using OpenC3::UI::Widgets::CmdTlmSnippets::isCmdTlmFile;
using OpenC3::UI::Widgets::CmdTlmSnippets::isPluginManifestFile;
using OpenC3::UI::Widgets::CmdTlmSnippets::isScreenFile;
using OpenC3::UI::Widgets::CmdTlmSnippets::isScriptFile;

// Matches `dir` as a path component regardless of whether `path` is
// plugin-root-relative (e.g. "cmd_tlm/foo.txt", no leading separator - what
// listPluginFiles()'s remote `find | sed` pipeline actually returns) or an
// absolute path (e.g. ".../cosmos-myplugin/cmd_tlm/foo.txt").
bool pathHasDir(const QString& path, const QString& dir)
{
    return path.contains("/" + dir + "/", Qt::CaseInsensitive)
        || path.startsWith(dir + "/", Qt::CaseInsensitive);
}

QString componentKind(const QString& path)
{
    if (path == "plugin.txt") return PluginView::tr("Plugin");
    if (path.endsWith(".gemspec", Qt::CaseInsensitive)) return PluginView::tr("Gemspec");
    if (pathHasDir(path, "cmd_tlm")) return PluginView::tr("CMD/TLM");
    if (pathHasDir(path, "screens")) return PluginView::tr("Screen");
    if (pathHasDir(path, "procedures")) return PluginView::tr("Procedure");
    return PluginView::tr("File");
}

int componentCategory(const QString& path)
{
    if (pathHasDir(path, "cmd_tlm")) return 0;
    if (pathHasDir(path, "screens")) return 1;
    if (pathHasDir(path, "procedures")) return 2;
    if (path == "plugin.txt" || path.endsWith(".gemspec", Qt::CaseInsensitive)) return 3;
    return 4;
}

QString componentCategoryTitle(int category)
{
    switch (category) {
    case 0: return PluginView::tr("CMD / TLM Definitions");
    case 1: return PluginView::tr("Screens");
    case 2: return PluginView::tr("Procedures");
    case 3: return PluginView::tr("Plugin Metadata");
    default: return PluginView::tr("Other Files");
    }
}

QString syntaxGuideText()
{
    return PluginView::tr(
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

// Matches CmdTlmFieldDialog's own name validation - the generated line is
// whitespace-tokenized when re-parsed (see CmdTlmParser), so a name
// containing a space or other COSMOS syntax character would silently shift
// every field after it instead of failing loudly. applyStructureRowToEditor()
// edits the Structure table's Field cell directly (bypassing that dialog
// entirely), so it needs the same check.
bool isValidFieldName(const QString& name)
{
    static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return pattern.match(name).hasMatch();
}

bool startsWithAnyKeyword(const QString& trimmed, const QStringList& keywords)
{
    for (const QString& keyword : keywords) {
        if (trimmed == keyword || trimmed.startsWith(keyword + " "))
            return true;
    }
    return false;
}

[[maybe_unused]] bool isBlockOrFieldHeader(const QString& line)
{
    static const QStringList keywords = {
        "COMMAND", "TELEMETRY",
        "APPEND_PARAMETER", "PARAMETER",
        "APPEND_ID_PARAMETER", "ID_PARAMETER",
        "APPEND_ITEM", "ITEM",
        "APPEND_ID_ITEM", "ID_ITEM",
        "APPEND_ARRAY_PARAMETER", "ARRAY_PARAMETER",
        "APPEND_ARRAY_ITEM", "ARRAY_ITEM"
    };
    return startsWithAnyKeyword(line.trimmed().toUpper(), keywords);
}

// Pull the produced gem filename out of a `gem build` log. The build command
// appends the `.gem` path, and gem itself prints a "File: <name>.gem" line.
QString extractGemPath(const QString& buildOutput)
{
    const QStringList lines = buildOutput.split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString line = it->trimmed();
        if (line.endsWith(".gem", Qt::CaseInsensitive)) {
            const qsizetype fileIdx = line.indexOf("File:", 0, Qt::CaseInsensitive);
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
    ViewModels::ValidatorViewModel& validatorVm,
    ViewModels::LogViewerViewModel& logViewerVm,
    QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
    , infraVm_(infraVm)
    , cmdTlmVm_(cmdTlmVm)
    , validatorVm_(validatorVm)
    , logViewerVm_(logViewerVm)
{
    setupUi();
    bindViewModel();

    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &PluginView::onSaveComponentClicked);

    // Restore the Terminal/Reference panel toggle state from the last
    // session (defaults match setupUi()'s initial hidden state above).
    QSettings settings;
    if (settings.value("PluginView/showReference", false).toBool()) {
        guideGroup_->setVisible(true);
        toggleReferenceBtn_->setChecked(true);
    }
    if (settings.value("PluginView/showTerminal", false).toBool()) {
        terminalPanel_->setVisible(true);
        toggleTerminalBtn_->setChecked(true);
    }
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
    auto* title = new QLabel(tr("Workspace"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setObjectName("PageTitle");
    auto* subtitle = new QLabel(
        tr("Select a plugin, edit CMD/TLM files, validate, build, and install."), this);
    subtitle->setObjectName("SubLabel");
    subtitle->setWordWrap(true);
    pageTitleLabel_ = title;
    pageSubtitleLabel_ = subtitle;
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    headerRow->addLayout(titleBlock, 1);

    scaffoldBtn_ = new QPushButton(tr("New Plugin"), this);
    addTargetBtn_ = new QPushButton(tr("Add Target"), this);
    validateBtn_ = new QPushButton(tr("Check"), this);
    buildBtn_ = new QPushButton(tr("Build Gem"), this);
    installBtn_ = new QPushButton(tr("Install Gem"), this);
    refreshBtn_ = new QPushButton(QString::fromUtf8("↻"), this);
    removeBtn_ = new QPushButton(tr("Remove"), this);

    removeBtn_->setEnabled(false);
    buildBtn_->setEnabled(false);
    addTargetBtn_->setEnabled(false);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    progressBar_->setFixedWidth(120);

    scaffoldBtn_->setToolTip(tr("Create a new plugin locally."));
    validateBtn_->setToolTip(tr("Check with openc3cli before building or installing."));
    buildBtn_->setToolTip(tr("Build the selected plugin into a gem. Save any open file first."));
    installBtn_->setToolTip(tr("Install a local .gem file into OpenC3."));
    refreshBtn_->setToolTip(tr("Refresh the local/remote plugin list."));

    // Add Target and Remove are secondary/less-frequent actions - kept as
    // real QPushButtons (so existing clicked-signal wiring is untouched) but
    // never added to a visible layout, so they must be hidden explicitly -
    // parented-but-unlaid-out widgets still render at their default position
    // otherwise. Their enabled/tooltip state is mirrored onto
    // addTargetAction_/removeAction_ below, which are what the user actually
    // sees and clicks, in the "More" menu.
    addTargetBtn_->setToolTip(tr("Add a target to the selected local plugin folder."));
    removeBtn_->setToolTip(tr("Remove the selected plugin from OpenC3."));
    addTargetBtn_->setVisible(false);
    removeBtn_->setVisible(false);

    buildBtn_->setObjectName("PrimaryButton");
    refreshBtn_->setObjectName("SecondaryIconButton");
    refreshBtn_->setFixedWidth(36);

    // Preferred (not Minimum + a hardcoded pixel floor) so each button's own
    // accurate sizeHint - which already accounts for the current font/DPI -
    // decides the true minimum. A fixed magic-number floor smaller than that
    // sizeHint is exactly what let text get clipped on other font/DPI setups.
    const QList<QPushButton*> topButtons = {
        scaffoldBtn_, validateBtn_, buildBtn_, installBtn_
    };
    for (auto* button : topButtons)
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Secondary actions (Add Target/Remove) live behind a "More" menu instead
    // of as always-visible buttons - same slots, less toolbar clutter.
    moreMenuBtn_ = new QToolButton(this);
    moreMenuBtn_->setText(tr("More") + " " + QString::fromUtf8("▾"));
    moreMenuBtn_->setToolTip(tr("Add a target or script, or remove this plugin."));
    moreMenuBtn_->setPopupMode(QToolButton::InstantPopup);
    auto* moreMenu = new QMenu(moreMenuBtn_);
    addTargetAction_ = moreMenu->addAction(tr("Add Target"));
    addScriptAction_ = moreMenu->addAction(tr("Add Script"));
    addScriptAction_->setToolTip(tr(
        "Add a procedures/<name>.rb script to an existing target - a beginner "
        "starter with a cmd()/wait_check() example."));
    removeAction_    = moreMenu->addAction(tr("Remove"));
    moreMenuBtn_->setMenu(moreMenu);
    moreMenuBtn_->setVisible(false);

    toggleTerminalBtn_ = new QPushButton(tr("Terminal"), this);
    toggleTerminalBtn_->setCheckable(true);
    toggleTerminalBtn_->setToolTip(tr("Show or hide the log-streaming terminal panel."));

    // Phase 6: the old top toolbar (New Plugin/Refresh/Check/Build/Install/
    // More) is gone now that every action lives on its own wizard step -
    // this header row keeps only cross-step chrome (Terminal toggle, busy
    // spinner) alongside the title.
    headerRow->addWidget(toggleTerminalBtn_);
    headerRow->addWidget(progressBar_);
    root->addLayout(headerRow);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("StatusBanner");
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    pluginSummaryLabel_ = new QLabel(tr("Select a plugin folder to inspect and edit its files."), this);
    pluginSummaryLabel_->setObjectName("SubLabel");
    pluginSummaryLabel_->setWordWrap(true);
    root->addWidget(pluginSummaryLabel_);

    // ── Wizard step strip ──────────────────────────────────────────────────────
    // Plugin -> File -> Edit -> Check -> Build & Install. This whole block
    // (step strip + page stack + Back/Next) is assembled into
    // wizardContentWidget, not added to root directly - it needs to sit
    // alongside terminalPanel_ in a vertical splitter (wired further down,
    // once terminalPanel_ exists) so the Terminal toggle keeps working
    // exactly as it did before the wizard redesign.
    auto* wizardContentWidget = new QWidget(this);
    auto* wizardContentLayout = new QVBoxLayout(wizardContentWidget);
    wizardContentLayout->setContentsMargins(0, 0, 0, 0);
    wizardContentLayout->setSpacing(10);

    // Phase 6: a persistent breadcrumb, visible regardless of which wizard
    // step is active, so context (which plugin/file is open) isn't lost when
    // jumping between steps.
    breadcrumbLabel_ = new QLabel(tr("No plugin selected"), this);
    breadcrumbLabel_->setObjectName("SubLabel");
    breadcrumbLabel_->setWordWrap(true);
    wizardContentLayout->addWidget(breadcrumbLabel_);

    auto* stepStripRow = new QHBoxLayout;
    stepStripRow->setSpacing(6);
    const QStringList stepLabels = {
        tr("1. Plugin"), tr("2. File"), tr("3. Edit"), tr("4. Check"), tr("5. Build && Install")
    };
    for (int i = 0; i < stepLabels.size(); ++i) {
        auto* stepBtn = new QPushButton(stepLabels[i], this);
        stepBtn->setObjectName("WizardStepButton");
        stepBtn->setCheckable(true);
        stepBtn->setAutoExclusive(true);
        connect(stepBtn, &QPushButton::clicked, this, [this, i] { goToWizardStep(i); });
        stepStripRow->addWidget(stepBtn);
        wizardStepButtons_.push_back(stepBtn);
    }
    stepStripRow->addStretch();
    wizardContentLayout->addLayout(stepStripRow);

    wizardStack_ = new QStackedWidget(this);
    QVBoxLayout* wizardPluginPageLayout = nullptr;
    QVBoxLayout* wizardFilePageLayout = nullptr;
    QVBoxLayout* wizardEditPageLayout = nullptr;
    QVBoxLayout* wizardCheckPageLayout = nullptr;
    QVBoxLayout* wizardBuildPageLayout = nullptr;
    for (int i = 0; i < stepLabels.size(); ++i) {
        auto* page = new QWidget(wizardStack_);
        auto* pageLayout = new QVBoxLayout(page);
        if (i == kWizardStepPlugin) {
            // Real content (plugin toolbar + table) is added into this page
            // further down, once those widgets exist - no placeholder here.
            wizardPluginPageLayout = pageLayout;
        } else if (i == kWizardStepFile) {
            wizardFilePageLayout = pageLayout;
        } else if (i == kWizardStepEdit) {
            wizardEditPageLayout = pageLayout;
        } else if (i == kWizardStepCheck) {
            wizardCheckPageLayout = pageLayout;
        } else if (i == kWizardStepBuild) {
            wizardBuildPageLayout = pageLayout;
        } else {
            auto* placeholder = new QLabel(
                tr("%1 (coming soon)").arg(stepLabels[i]), page);
            placeholder->setAlignment(Qt::AlignCenter);
            placeholder->setObjectName("SubLabel");
            pageLayout->addWidget(placeholder);
        }
        wizardStack_->addWidget(page);
    }
    wizardContentLayout->addWidget(wizardStack_, 1);

    auto* wizardNavRow = new QHBoxLayout;
    wizardBackBtn_ = new QPushButton(tr("< Back"), this);
    wizardNextBtn_ = new QPushButton(tr("Next >"), this);
    wizardNextBtn_->setObjectName("PrimaryButton");
    connect(wizardBackBtn_, &QPushButton::clicked, this, [this] { goToWizardStep(currentWizardStep_ - 1); });
    connect(wizardNextBtn_, &QPushButton::clicked, this, [this] { goToWizardStep(currentWizardStep_ + 1); });
    wizardNavRow->addStretch();
    wizardNavRow->addWidget(wizardBackBtn_);
    wizardNavRow->addWidget(wizardNextBtn_);
    wizardContentLayout->addLayout(wizardNavRow);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setObjectName("PluginWorkbench");

    auto* leftPane = new QWidget(mainSplitter);
    leftPane->setObjectName("PluginLeftPane");
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto* pluginListGroup = new QGroupBox(tr("Plugin Folders"), leftPane);
    auto* pluginListLayout = new QVBoxLayout(pluginListGroup);
    pluginListLayout->setContentsMargins(8, 14, 8, 8);
    tableView_ = new QTableView(pluginListGroup);
    tableView_->setObjectName("PluginTable");
    tableView_->setModel(vm_.pluginModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setShowGrid(false);
    tableView_->setSortingEnabled(true);
    tableView_->setMinimumHeight(240);
    tableView_->setColumnHidden(ViewModels::PluginTableModel::Version, true);
    tableView_->horizontalHeader()->setStretchLastSection(false);
    tableView_->horizontalHeader()->setSectionResizeMode(ViewModels::PluginTableModel::Name, QHeaderView::Stretch);
    tableView_->horizontalHeader()->setSectionResizeMode(ViewModels::PluginTableModel::Status, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(ViewModels::PluginTableModel::Author, QHeaderView::ResizeToContents);
    tableView_->setColumnWidth(ViewModels::PluginTableModel::Status, 96);
    tableView_->setColumnWidth(ViewModels::PluginTableModel::Author, 58);
    pluginListLayout->addWidget(tableView_, 1);

    // Wizard Phase 1/6: New Plugin/Refresh/More(Add Target/Remove) + the
    // Plugin Folders table live on the wizard's Plugin page, not the old
    // (now-hidden) leftPane. validateBtn_ (whole-plugin Check, distinct from
    // the per-file Check on the Check step) joins them here too, instead of
    // the old top toolbar it used to share with Build/Install.
    auto* pluginPageToolbar = new QHBoxLayout;
    pluginPageToolbar->setSpacing(8);
    pluginPageToolbar->addWidget(scaffoldBtn_);
    pluginPageToolbar->addWidget(refreshBtn_);
    pluginPageToolbar->addWidget(moreMenuBtn_);
    pluginPageToolbar->addStretch();
    pluginPageToolbar->addWidget(validateBtn_);
    wizardPluginPageLayout->addLayout(pluginPageToolbar);
    wizardPluginPageLayout->addWidget(pluginListGroup, 1);

    auto* componentListGroup = new QGroupBox(tr("Files"), leftPane);
    auto* componentListGroupLayout = new QVBoxLayout(componentListGroup);
    componentListGroupLayout->setContentsMargins(8, 14, 8, 8);

    filesTabs_ = new QTabWidget(componentListGroup);
    componentListGroupLayout->addWidget(filesTabs_);

    // ── "Plugin Files" tab: scoped to the selected plugin's root ─────────────
    auto* pluginFilesTab = new QWidget(filesTabs_);
    auto* componentListLayout = new QVBoxLayout(pluginFilesTab);
    componentListLayout->setContentsMargins(0, 8, 0, 0);
    componentListLayout->setSpacing(6);
    componentHintLabel_ = new QLabel(tr("Select a plugin first."), pluginFilesTab);
    componentHintLabel_->setObjectName("SubLabel");
    componentHintLabel_->setWordWrap(true);
    componentListEmptyLabel_ = new QLabel(tr("Select a plugin folder to list editable plugin files."), pluginFilesTab);
    componentListEmptyLabel_->setObjectName("SubLabel");
    componentListEmptyLabel_->setWordWrap(true);
    componentList_ = new QListWidget(pluginFilesTab);
    componentList_->setObjectName("PluginComponentList");
    componentList_->setMinimumWidth(280);
    componentListLayout->addWidget(componentHintLabel_);
    componentListLayout->addWidget(componentListEmptyLabel_);
    componentListLayout->addWidget(componentList_, 1);
    filesTabs_->addTab(pluginFilesTab, tr("Plugin Files"));

    // ── "Browse" tab: unrestricted remote directory browsing ─────────────────
    auto* browseTab = new QWidget(filesTabs_);
    auto* browseLayout = new QVBoxLayout(browseTab);
    browseLayout->setContentsMargins(0, 8, 0, 0);
    browseLayout->setSpacing(6);
    auto* browsePathRow = new QHBoxLayout;
    browsePathRow->setSpacing(6);
    browsePathEdit_ = new QLineEdit(browseTab);
    browsePathEdit_->setPlaceholderText("/cosmos/targets");
    browseGoBtn_ = new QPushButton(tr("Go"), browseTab);
    browsePathRow->addWidget(browsePathEdit_, 1);
    browsePathRow->addWidget(browseGoBtn_);
    browseEmptyLabel_ = new QLabel(
        tr("Browse any remote path - not limited to the selected plugin."), browseTab);
    browseEmptyLabel_->setObjectName("SubLabel");
    browseEmptyLabel_->setWordWrap(true);
    browseList_ = new QListWidget(browseTab);
    browseList_->setObjectName("PluginBrowseList");
    browsePathEdit_->setText(cmdTlmVm_.isConnected()
        ? cmdTlmVm_.defaultCmdTlmPath()
        : QStringLiteral("/cosmos/targets"));
    browseGoBtn_->setEnabled(cmdTlmVm_.isConnected());
    browsePathEdit_->setEnabled(cmdTlmVm_.isConnected());
    browseLayout->addLayout(browsePathRow);
    browseLayout->addWidget(browseEmptyLabel_);
    browseLayout->addWidget(browseList_, 1);
    filesTabs_->addTab(browseTab, tr("Browse"));

    // Wizard Phase 2: the Files group (Plugin Files/Browse tabs) lives on
    // the wizard's File page instead of the old (now-hidden) leftPane.
    wizardFilePageLayout->addWidget(componentListGroup, 1);
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
    detailTabs_->addTab(detailTab, tr("Overview"));

    // Formerly detailTabs_'s "File" tab content - now lives on the wizard's
    // Edit page (wizardEditPageLayout, below) instead, so no wrapping
    // componentTab/componentLayout is needed anymore.
    auto* editorPane = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);
    // Without an explicit floor here, nothing stops the vertical splitter
    // (see terminalPanel_ below) from shrinking this pane's toolbar rows and
    // the Structure editor's fields below their own minimum size hints -
    // QSplitter compresses past a child's minimumSizeHint rather than
    // guaranteeing it, which rendered rows squashed on top of each other at
    // ordinary window heights once the Terminal panel was toggled on. This
    // forces the window itself to stay tall enough instead.
    editorPane->setMinimumHeight(520);

    auto* componentToolbarBlock = new QVBoxLayout;
    componentToolbarBlock->setSpacing(6);
    auto* componentPathRow = new QHBoxLayout;
    componentPathRow->setSpacing(8);
    auto* componentActionRow = new QHBoxLayout;
    componentActionRow->setSpacing(6);
    auto* componentEditRow = new QHBoxLayout;
    componentEditRow->setSpacing(6);
    componentPathLabel_ = new QLabel(tr("Select a plugin file"), editorPane);
    componentPathLabel_->setObjectName("PluginFilePath");
    openComponentBtn_ = new QPushButton(tr("Open"), editorPane);
    saveComponentBtn_ = new QPushButton(tr("Save"), editorPane);
    validateComponentBtn_ = new QPushButton(tr("Check"), editorPane);
    validateOfflineBtn_ = new QPushButton(tr("Offline Check"), editorPane);
    startCmdTlmEditBtn_ = new QPushButton(tr("Edit CMD/TLM"), editorPane);
    insertCmdBtn_ = new QPushButton(tr("+ COMMAND"), editorPane);
    insertTlmBtn_ = new QPushButton(tr("+ TELEMETRY"), editorPane);
    insertScriptBtn_ = new QPushButton(tr("+ SCRIPT STEP"), editorPane);
    runScriptBtn_ = new QPushButton(tr("Run Script"), editorPane);
    runScriptBtn_->setToolTip(tr(
        "Runs this file with `ruby` on the connected host (cd <plugin root> && "
        "ruby <path>) - not the full COSMOS Script Runner, so cmd()/tlm() only "
        "work if the openc3 script libraries are loadable there."));
    checkScriptSyntaxBtn_ = new QPushButton(tr("Check Syntax"), editorPane);
    checkScriptSyntaxBtn_->setToolTip(tr(
        "Runs `ruby -c` on the saved file - catches Ruby syntax errors "
        "without needing a live COSMOS connection or the openc3 gem."));
    addFieldBtn_ = new QPushButton(tr("Add Field"), editorPane);
    addStructureFieldBtn_ = new QPushButton(tr("Add Row"), editorPane);
    deleteStructureFieldBtn_ = new QPushButton(tr("Delete Row"), editorPane);
    refreshStructureBtn_ = new QPushButton(tr("Refresh"), editorPane);
    applyStructureBtn_ = new QPushButton(tr("Apply"), editorPane);
    toggleReferenceBtn_ = new QPushButton(tr("Reference"), editorPane);
    openComponentBtn_->setText(tr("Open"));
    saveComponentBtn_->setText(tr("Save"));
    validateComponentBtn_->setText(tr("Check"));
    validateComponentBtn_->setToolTip(tr("Check this file and refresh the structure view."));
    validateOfflineBtn_->setText(tr("Offline Check"));
    validateOfflineBtn_->setToolTip(
        tr("Full per-rule offline check in the Validator view."));
    startCmdTlmEditBtn_->setText(tr("Edit CMD/TLM"));
    startCmdTlmEditBtn_->setToolTip(
        tr("Open the first CMD/TLM file in this plugin and start the common edit flow."));
    addFieldBtn_->setText(tr("Add Field"));
    addFieldBtn_->setToolTip(tr("Add a field to the current CMD/TLM file."));
    addStructureFieldBtn_->setText(tr("Add Row"));
    addStructureFieldBtn_->setToolTip(tr("Add a structure row to the current CMD/TLM file."));
    deleteStructureFieldBtn_->setText(tr("Delete Row"));
    deleteStructureFieldBtn_->setToolTip(tr("Delete the selected structure row."));
    refreshStructureBtn_->setText(tr("Refresh"));
    refreshStructureBtn_->setToolTip(tr("Re-read the file into the structure editor."));
    applyStructureBtn_->setText(tr("Apply"));
    applyStructureBtn_->setToolTip(tr("Apply the selected structure change to the editor buffer."));
    toggleReferenceBtn_->setText(tr("Reference"));
    toggleReferenceBtn_->setCheckable(true);
    toggleReferenceBtn_->setToolTip(tr("Show or hide the CMD/TLM quick reference side panel."));

    validateMenuBtn_ = new QToolButton(editorPane);
    validateMenuBtn_->setText(tr("Check") + " ▾");
    validateMenuBtn_->setToolTip(tr("More ways to check this file."));
    validateMenuBtn_->setPopupMode(QToolButton::InstantPopup);
    auto* validateMenu = new QMenu(validateMenuBtn_);
    validateOfflineAction_ = validateMenu->addAction(tr("Offline Check"));
    validateMenuBtn_->setMenu(validateMenu);

    insertMenuBtn_ = new QToolButton(editorPane);
    insertMenuBtn_->setText(tr("Insert") + " ▾");
    insertMenuBtn_->setToolTip(tr("Insert a COMMAND/TELEMETRY template or a field."));
    insertMenuBtn_->setPopupMode(QToolButton::InstantPopup);
    auto* insertMenu = new QMenu(insertMenuBtn_);
    insertCmdAction_ = insertMenu->addAction(tr("COMMAND"));
    insertTlmAction_ = insertMenu->addAction(tr("TELEMETRY"));
    addFieldAction_ = insertMenu->addAction(tr("Add Field"));
    insertScriptAction_ = insertMenu->addAction(tr("Script Step (cmd/wait_check)"));
    insertMenuBtn_->setMenu(insertMenu);

    structureMenuBtn_ = new QToolButton(editorPane);
    structureMenuBtn_->setText(tr("Fields") + " ▾");
    structureMenuBtn_->setToolTip(tr("Edit structure rows in the raw text."));
    structureMenuBtn_->setPopupMode(QToolButton::InstantPopup);
    auto* structureMenu = new QMenu(structureMenuBtn_);
    addStructureFieldAction_ = structureMenu->addAction(tr("Add Row"));
    deleteStructureFieldAction_ = structureMenu->addAction(tr("Delete Row"));
    refreshStructureAction_ = structureMenu->addAction(tr("Refresh"));
    applyStructureAction_ = structureMenu->addAction(tr("Apply"));
    structureMenuBtn_->setMenu(structureMenu);

    auto* fileOpenRow = new QHBoxLayout;
    fileOpenRow->setSpacing(6);
    fileOpenRow->addWidget(openComponentBtn_);
    fileOpenRow->addStretch();
    componentListLayout->addLayout(fileOpenRow);
    startCmdTlmEditBtn_->setObjectName("PrimaryButton");
    saveComponentBtn_->setObjectName("PrimaryButton");
    openComponentBtn_->setEnabled(false);
    saveComponentBtn_->setEnabled(false);
    validateComponentBtn_->setEnabled(false);
    validateOfflineBtn_->setEnabled(false);
    startCmdTlmEditBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    insertScriptBtn_->setEnabled(false);
    runScriptBtn_->setEnabled(false);
    checkScriptSyntaxBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);
    addStructureFieldBtn_->setEnabled(false);
    deleteStructureFieldBtn_->setEnabled(false);
    refreshStructureBtn_->setEnabled(false);
    applyStructureBtn_->setEnabled(false);
    updateComponentEmptyState();
    // No hardcoded pixel-width floors on the buttons below that are actually
    // shown in the UI (openComponentBtn_/saveComponentBtn_/
    // validateComponentBtn_/startCmdTlmEditBtn_) - each QPushButton's own
    // sizeHint already reserves exactly the space its current text needs for
    // the active font/DPI, so it can never clip. The remaining buttons here
    // (validateOfflineBtn_, insertCmdBtn_, insertTlmBtn_, insertScriptBtn_,
    // addFieldBtn_, addStructureFieldBtn_, deleteStructureFieldBtn_,
    // refreshStructureBtn_, applyStructureBtn_) are never added to a layout - they only back the
    // enabled/tooltip state mirrored onto the equivalent QAction in the
    // Check/Insert/Fields dropdown menus - so their width is irrelevant.
    validateOfflineBtn_->setMinimumWidth(110);
    insertCmdBtn_->setMinimumWidth(110);
    insertTlmBtn_->setMinimumWidth(120);
    addFieldBtn_->setMinimumWidth(110);
    refreshStructureBtn_->setMinimumWidth(130);
    applyStructureBtn_->setMinimumWidth(150);

    componentPathRow->addWidget(new QLabel(tr("Selected file:"), editorPane));
    componentPathRow->addWidget(componentPathLabel_, 1);
    componentActionRow->addWidget(saveComponentBtn_);
    componentActionRow->addWidget(startCmdTlmEditBtn_);
    componentActionRow->addWidget(runScriptBtn_);
    componentActionRow->addWidget(checkScriptSyntaxBtn_);
    componentActionRow->addStretch();
    // validateComponentBtn_/validateMenuBtn_ (Check/Check▾) move to the
    // wizard's Check page below instead of staying on this row - Phase 4.
    componentEditRow->addWidget(insertMenuBtn_);
    componentEditRow->addWidget(structureMenuBtn_);
    componentEditRow->addStretch();
    componentEditRow->addWidget(toggleReferenceBtn_);
    componentToolbarBlock->addLayout(componentPathRow);
    componentToolbarBlock->addLayout(componentActionRow);
    componentToolbarBlock->addLayout(componentEditRow);
    editorLayout->addLayout(componentToolbarBlock);

    componentEditor_ = new QTextEdit(editorPane);
    componentEditor_->setObjectName("CodeEditor");
    componentEditor_->setLineWrapMode(QTextEdit::NoWrap);
    componentEditor_->setPlaceholderText(tr("Open a plugin file to edit it here."));
    componentEditor_->setMinimumHeight(220);
    highlighter_ = new Widgets::CmdTlmHighlighter(componentEditor_->document());

    componentEditorTabs_ = new QTabWidget(editorPane);
    componentEditorTabs_->setObjectName("PluginDetailTabs");

    auto* structureTab = new QWidget(componentEditorTabs_);
    auto* structureTabLayout = new QVBoxLayout(structureTab);
    structureTabLayout->setContentsMargins(0, 0, 0, 0);

    auto* structureGroup = new QGroupBox(tr("Structure Editor (Block / Field)"), editorPane);
    auto* structureLayout = new QVBoxLayout(structureGroup);

    // ── Block editor (COMMAND / TELEMETRY header) ────────────────────────────
    // Lets the user retarget / rename / re-describe a whole packet without
    // touching the raw COMMAND/TELEMETRY line by hand.
    auto* blockSelectRow = new QHBoxLayout;
    blockSelectRow->setSpacing(6);
    blockSelectRow->addWidget(new QLabel(tr("Block:"), structureGroup));
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
    blockDescriptionEdit_->setPlaceholderText(tr("Description"));
    applyBlockBtn_ = new QPushButton(tr("Apply Block"), structureGroup);
    applyBlockBtn_->setToolTip(tr("Updates the selected COMMAND/TELEMETRY line. "
                              "Comments and child fields are preserved."));
    // Unlike the other buttons on this row, this one has no default width
    // floor - once the Reference panel or a narrower window squeezes this
    // row, QPushButton clips its own label ("pply Bloc") instead of eliding.
    applyBlockBtn_->setMinimumWidth(applyBlockBtn_->sizeHint().width());
    blockFieldRow->addWidget(new QLabel(tr("Target:"), structureGroup));
    blockFieldRow->addWidget(blockTargetEdit_);
    blockFieldRow->addWidget(new QLabel(tr("Name:"), structureGroup));
    blockFieldRow->addWidget(blockNameEdit_);
    blockFieldRow->addWidget(new QLabel(tr("Endian:"), structureGroup));
    blockFieldRow->addWidget(blockEndiannessCombo_);
    blockFieldRow->addWidget(new QLabel(tr("Desc:"), structureGroup));
    blockFieldRow->addWidget(blockDescriptionEdit_, 1);
    blockFieldRow->addWidget(applyBlockBtn_);
    structureLayout->addLayout(blockFieldRow);

    blockSelectorCombo_->setEnabled(false);
    blockTargetEdit_->setEnabled(false);
    blockNameEdit_->setEnabled(false);
    blockEndiannessCombo_->setEnabled(false);
    blockDescriptionEdit_->setEnabled(false);
    applyBlockBtn_->setEnabled(false);

    structureTable_ = new QTableWidget(0, 11, structureGroup);
    structureTable_->setObjectName("PluginStructureTable");
    structureTable_->setHorizontalHeaderLabels({
        tr("Line"), tr("Block"), tr("Field"), tr("Offset"), tr("Bits"), tr("Type"), tr("Array Bits"),
        tr("Min"), tr("Max"), tr("Default"), tr("Description")
    });
    structureTable_->horizontalHeader()->setStretchLastSection(true);
    structureTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // Block repeats the fully-qualified "TELEMETRY TARGET::PACKET" name on every
    // row (real target/packet names run much longer than the mock "MYSAT"), so
    // ResizeToContents on it can consume most of the table's width and squeeze
    // Description down to nothing. Cap it to a fixed, user-resizable width and
    // rely on the item's tooltip (set in refreshStructureTable()) for the full
    // name instead.
    structureTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    structureTable_->setColumnWidth(1, 170);
    structureTable_->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Stretch);
    structureTable_->verticalHeader()->setVisible(false);
    structureTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    structureTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    structureTable_->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::SelectedClicked);
    structureTable_->setMinimumHeight(260);
    structureTable_->setColumnWidth(0, 48);   // Line
    structureLayout->addWidget(structureTable_, 1);
    structureTabLayout->addWidget(structureGroup, 1);
    componentDiagnostics_ = new QTextEdit(editorPane);
    componentDiagnostics_->setObjectName("LogArea");
    componentDiagnostics_->setReadOnly(true);
    componentDiagnostics_->setMaximumHeight(50);
    componentDiagnostics_->setPlaceholderText(tr("Validation results will appear here."));

    diagnosticList_ = new QListWidget(editorPane);
    diagnosticList_->setObjectName("PluginDiagnosticList");
    // Trimmed from 150 - on a window short enough to squeeze the Structure
    // table (e.g. after subtracting real OS window chrome/taskbar height,
    // which this app's own default-size testing doesn't account for), this
    // list's own scrollbar already handles overflow, so it doesn't need to
    // reserve as much fixed space as the table above it.
    diagnosticList_->setMaximumHeight(110);
    diagnosticListEmptyLabel_ = new QLabel(tr("No diagnostics yet - check the file to see issues here."), editorPane);
    diagnosticListEmptyLabel_->setObjectName("SubLabel");
    diagnosticListEmptyLabel_->setWordWrap(true);

    auto* manifestTab = new QWidget(componentEditorTabs_);
    auto* manifestTabLayout = new QVBoxLayout(manifestTab);
    manifestTabLayout->setContentsMargins(0, 0, 0, 0);

    auto* manifestGroup = new QGroupBox(
        tr("Plugin Manifest (Target / Interface / Router / Microservice / Tool / Widget / Variable)"),
        editorPane);
    auto* manifestLayout = new QVBoxLayout(manifestGroup);

    // ── Block editor (TARGET/INTERFACE/ROUTER/MICROSERVICE/TOOL/WIDGET/
    // VARIABLE header line) - mirrors the Structure tab's block editor above.
    auto* manifestBlockSelectRow = new QHBoxLayout;
    manifestBlockSelectRow->setSpacing(6);
    manifestBlockSelectRow->addWidget(new QLabel(tr("Block:"), manifestGroup));
    manifestBlockSelectorCombo_ = new QComboBox(manifestGroup);
    manifestBlockSelectorCombo_->setObjectName("PluginManifestBlockCombo");
    manifestBlockSelectorCombo_->setMinimumWidth(220);
    manifestBlockSelectorCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    manifestKindLabel_ = new QLabel("", manifestGroup);
    manifestKindLabel_->setObjectName("SubLabel");
    manifestBlockSelectRow->addWidget(manifestBlockSelectorCombo_, 1);
    manifestBlockSelectRow->addWidget(manifestKindLabel_);
    manifestLayout->addLayout(manifestBlockSelectRow);

    auto* manifestBlockFieldRow = new QHBoxLayout;
    manifestBlockFieldRow->setSpacing(6);
    manifestNameEdit_ = new QLineEdit(manifestGroup);
    manifestNameEdit_->setPlaceholderText(tr("NAME"));
    manifestClassOrFolderEdit_ = new QLineEdit(manifestGroup);
    manifestClassOrFolderEdit_->setPlaceholderText(tr("FOLDER / CLASS FILE"));
    manifestArgsEdit_ = new QLineEdit(manifestGroup);
    manifestArgsEdit_->setPlaceholderText(tr("Additional arguments"));
    applyManifestBlockBtn_ = new QPushButton(tr("Apply Block"), manifestGroup);
    applyManifestBlockBtn_->setToolTip(tr(
        "Updates the selected block's header line. Modifier lines are preserved."));
    applyManifestBlockBtn_->setMinimumWidth(applyManifestBlockBtn_->sizeHint().width());
    manifestBlockFieldRow->addWidget(new QLabel(tr("Name:"), manifestGroup));
    manifestBlockFieldRow->addWidget(manifestNameEdit_);
    manifestBlockFieldRow->addWidget(new QLabel(tr("Folder/Class:"), manifestGroup));
    manifestBlockFieldRow->addWidget(manifestClassOrFolderEdit_);
    manifestBlockFieldRow->addWidget(new QLabel(tr("Args:"), manifestGroup));
    manifestBlockFieldRow->addWidget(manifestArgsEdit_, 1);
    manifestBlockFieldRow->addWidget(applyManifestBlockBtn_);
    manifestLayout->addLayout(manifestBlockFieldRow);

    manifestBlockSelectorCombo_->setEnabled(false);
    manifestNameEdit_->setEnabled(false);
    manifestClassOrFolderEdit_->setEnabled(false);
    manifestArgsEdit_->setEnabled(false);
    applyManifestBlockBtn_->setEnabled(false);

    manifestMenuBtn_ = new QToolButton(manifestGroup);
    manifestMenuBtn_->setText(tr("Manifest") + " ▾");
    manifestMenuBtn_->setToolTip(tr(
        "Add a new block, or add/delete a modifier line (PROTOCOL, ENV, SECRET, ...)."));
    manifestMenuBtn_->setPopupMode(QToolButton::InstantPopup);
    auto* manifestMenu = new QMenu(manifestMenuBtn_);

    // TARGET is deliberately excluded - it already has its own dedicated
    // creation flow ("Add Target", AddTargetDialog), which also scaffolds
    // the target's folder structure, not just a plugin.txt line.
    auto* newBlockMenu = manifestMenu->addMenu(tr("New Block"));
    auto* newInterfaceAction = newBlockMenu->addAction(tr("Interface"));
    auto* newRouterAction = newBlockMenu->addAction(tr("Router"));
    auto* newMicroserviceAction = newBlockMenu->addAction(tr("Microservice"));
    auto* newToolAction = newBlockMenu->addAction(tr("Tool (Advanced)"));
    auto* newWidgetAction = newBlockMenu->addAction(tr("Widget (Advanced)"));
    newToolAction->setToolTip(tr(
        "A Tool needs a real web frontend (Vue.js) you write outside this app - "
        "this only adds its plugin.txt declaration."));
    newWidgetAction->setToolTip(tr(
        "A Widget needs real frontend code you write outside this app - "
        "this only adds its plugin.txt declaration."));
    auto* newVariableAction = newBlockMenu->addAction(tr("Variable"));
    connect(newInterfaceAction, &QAction::triggered, this, [this] { onNewManifestInterfaceOrRouter(false); });
    connect(newRouterAction, &QAction::triggered, this, [this] { onNewManifestInterfaceOrRouter(true); });
    connect(newMicroserviceAction, &QAction::triggered, this, &PluginView::onNewManifestMicroservice);
    connect(newToolAction, &QAction::triggered, this, [this] {
        if (confirmAdvancedFrontendBlock(tr("Tool")))
            appendManifestBlockSnippet(Widgets::PluginManifestSnippets::toolBlock()); });
    connect(newWidgetAction, &QAction::triggered, this, [this] {
        if (confirmAdvancedFrontendBlock(tr("Widget")))
            appendManifestBlockSnippet(Widgets::PluginManifestSnippets::widgetBlock()); });
    connect(newVariableAction, &QAction::triggered, this, [this] {
        appendManifestBlockSnippet(Widgets::PluginManifestSnippets::variableBlock()); });

    manifestMenu->addSeparator();
    addManifestModifierAction_ = manifestMenu->addAction(tr("Add Modifier"));
    deleteManifestModifierAction_ = manifestMenu->addAction(tr("Delete Modifier"));
    refreshManifestAction_ = manifestMenu->addAction(tr("Refresh"));
    manifestMenuBtn_->setMenu(manifestMenu);
    auto* manifestMenuRow = new QHBoxLayout;
    manifestMenuRow->addWidget(manifestMenuBtn_);
    manifestMenuRow->addStretch();
    manifestLayout->addLayout(manifestMenuRow);

    manifestTable_ = new QTableWidget(0, 4, manifestGroup);
    manifestTable_->setObjectName("PluginManifestTable");
    manifestTable_->setHorizontalHeaderLabels({
        tr("Line"), tr("Block / Keyword"), tr("Args"), tr("Kind")
    });
    manifestTable_->horizontalHeader()->setStretchLastSection(false);
    manifestTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    manifestTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    manifestTable_->verticalHeader()->setVisible(false);
    manifestTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    manifestTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    // Only modifier rows' Keyword/Args cells are made editable (in
    // refreshManifestTable()) - block header rows stay read-only here since
    // they're edited via the form above instead.
    manifestTable_->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::SelectedClicked);
    manifestTable_->setMinimumHeight(220);
    manifestTable_->setColumnWidth(0, 48);   // Line
    manifestLayout->addWidget(manifestTable_, 1);
    manifestTabLayout->addWidget(manifestGroup, 1);

    auto* previewTab = new QWidget(componentEditorTabs_);
    auto* previewTabLayout = new QVBoxLayout(previewTab);
    previewTabLayout->setContentsMargins(0, 0, 0, 0);

    auto* previewBanner = new QLabel(
        tr("Layout preview - static placeholders, not live telemetry."), previewTab);
    previewBanner->setObjectName("SubLabel");
    previewBanner->setWordWrap(true);
    previewTabLayout->addWidget(previewBanner);

    auto* previewToolbarRow = new QHBoxLayout;
    addScreenWidgetBtn_ = new QPushButton(tr("Add Widget"), previewTab);
    addScreenWidgetBtn_->setToolTip(tr(
        "Add a Title, Label, Value, or Button to the end of this screen."));
    previewToolbarRow->addWidget(addScreenWidgetBtn_);
    previewToolbarRow->addStretch();
    previewTabLayout->addLayout(previewToolbarRow);

    auto* previewScroll = new QScrollArea(previewTab);
    previewScroll->setObjectName("ScreenPreviewScrollArea");
    previewScroll->setWidgetResizable(true);
    screenPreview_ = new Widgets::ScreenPreviewWidget(previewScroll);
    previewScroll->setWidget(screenPreview_);
    previewTabLayout->addWidget(previewScroll, 1);

    componentEditorTabs_->addTab(componentEditor_, tr("Source"));
    componentEditorTabs_->addTab(structureTab, tr("Structure"));
    componentEditorTabs_->addTab(manifestTab, tr("Manifest"));
    componentEditorTabs_->addTab(previewTab, tr("Preview"));

    guideGroup_ = new QGroupBox(tr("Quick Reference"), editorPane);
    auto* guideLayout = new QVBoxLayout(guideGroup_);
    componentGuide_ = new QTextEdit(guideGroup_);
    componentGuide_->setObjectName("LogArea");
    componentGuide_->setReadOnly(true);
    componentGuide_->setLineWrapMode(QTextEdit::WidgetWidth);
    componentGuide_->setPlainText(syntaxGuideText());
    guideLayout->addWidget(componentGuide_);

    auto* centerSplitter = new QSplitter(Qt::Horizontal, editorPane);
    centerSplitter->addWidget(componentEditorTabs_);
    centerSplitter->addWidget(guideGroup_);
    centerSplitter->setStretchFactor(0, 1);
    centerSplitter->setStretchFactor(1, 0);
    centerSplitter->setSizes({760, 260});
    guideGroup_->setVisible(false);

    editorLayout->addWidget(centerSplitter, 1);
    // Wizard Phase 3: the whole editor pane (path/toolbar + Source/Structure
    // tabs + Reference splitter) lives on the wizard's Edit page instead of
    // the old (now-hidden) detailTabs_ "File" tab.
    wizardEditPageLayout->addWidget(editorPane, 1);

    // Wizard Phase 4: Check/Offline Check + the validation summary and
    // itemized diagnostics list live on their own Check page - promoted
    // to page-level primary actions instead of small buttons buried in the
    // Edit page's toolbar.
    auto* checkPageToolbar = new QHBoxLayout;
    checkPageToolbar->setSpacing(8);
    checkPageToolbar->addWidget(validateComponentBtn_);
    checkPageToolbar->addWidget(validateMenuBtn_);
    checkPageToolbar->addStretch();
    validateComponentBtn_->setObjectName("PrimaryButton");
    wizardCheckPageLayout->addLayout(checkPageToolbar);
    wizardCheckPageLayout->addWidget(new QLabel(tr("Validation summary"), this));
    wizardCheckPageLayout->addWidget(componentDiagnostics_);
    wizardCheckPageLayout->addWidget(diagnosticListEmptyLabel_);
    wizardCheckPageLayout->addWidget(diagnosticList_, 1);

    // Wizard Phase 5: Build Gem/Install Gem move from the old top toolbar
    // (selectedPluginActions_) to their own Build & Install page, alongside
    // detailTabs_/detailEdit_ - the existing build/install output view,
    // previously hidden inside the now-vestigial mainSplitter.
    auto* buildPageToolbar = new QHBoxLayout;
    buildPageToolbar->setSpacing(8);
    buildPageToolbar->addWidget(buildBtn_);
    buildPageToolbar->addWidget(installBtn_);
    buildPageToolbar->addStretch();
    wizardBuildPageLayout->addLayout(buildPageToolbar);
    wizardBuildPageLayout->addWidget(detailTabs_, 1);
    validateOfflineBtn_->setVisible(false);
    insertCmdBtn_->setVisible(false);
    insertTlmBtn_->setVisible(false);
    insertScriptBtn_->setVisible(false);
    addFieldBtn_->setVisible(false);
    addStructureFieldBtn_->setVisible(false);
    deleteStructureFieldBtn_->setVisible(false);
    refreshStructureBtn_->setVisible(false);
    applyStructureBtn_->setVisible(false);
    setCmdTlmActionsVisible(false);
    startCmdTlmEditBtn_->setVisible(false);

    // mainSplitter (leftPane/detailTabs_) is legacy scaffolding left over
    // from before the wizard redesign - every real widget it used to carry
    // has been moved into a wizard page by now, so it's just parented-but-
    // unlaid-out (same pattern already used for addTargetBtn_/removeBtn_
    // above) rather than added anywhere visible.
    mainSplitter->addWidget(leftPane);
    mainSplitter->addWidget(detailTabs_);
    mainSplitter->setVisible(false);

    // Bottom "Terminal" panel: the existing Logs screen embedded as a child
    // widget, bound to the same shared logViewerVm_ - reuses its streaming
    // logic entirely. Hidden by default so the screen isn't more cluttered
    // than today; toggled via toggleTerminalBtn_. Paired with the wizard
    // content (not mainSplitter) in this vertical splitter so the Terminal
    // toggle keeps working now that the wizard is the real visible content.
    auto* verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->setObjectName("PluginWorkbenchWithTerminal");
    terminalPanel_ = new QWidget(verticalSplitter);
    auto* terminalLayout = new QVBoxLayout(terminalPanel_);
    terminalLayout->setContentsMargins(0, 0, 0, 0);
    terminalView_ = new LogViewerView(logViewerVm_, terminalPanel_);
    terminalLayout->addWidget(terminalView_);
    verticalSplitter->addWidget(wizardContentWidget);
    verticalSplitter->addWidget(terminalPanel_);
    verticalSplitter->setChildrenCollapsible(false);
    verticalSplitter->setStretchFactor(0, 1);
    verticalSplitter->setStretchFactor(1, 0);
    // The editor pane's own toolbar rows + Structure table already need more
    // height than a 220px terminal split left it - at a typical window height
    // that squeezed every row in the toolbar down past its own minimum size,
    // rendering rows so tight they visually overlapped. Give the terminal a
    // smaller, still-usable default share and a firm floor so it - not the
    // primary editor content - absorbs the space pressure.
    terminalPanel_->setMinimumHeight(160);
    verticalSplitter->setSizes({760, 180});
    terminalPanel_->setVisible(false);
    root->addWidget(verticalSplitter, 1);

    // Must run after structureTable_ (created above) exists: updateActionHints()
    // reads its selection state. Calling this earlier segfaults on a null
    // structureTable_ during construction - every other widget it touches is
    // created earlier in this function, but this one is not.
    updateActionHints();

    goToWizardStep(kWizardStepPlugin);
}

int PluginView::maxReachableWizardStep() const
{
    if (selectedPluginName().isEmpty())
        return kWizardStepPlugin;
    if (currentComponentPath_.isEmpty())
        return kWizardStepFile;
    return kWizardStepBuild;
}

bool PluginView::stepVisibleInCurrentMode(int step) const noexcept
{
    return stepStripMode_ == StepStripMode::Creation
        ? step <= kWizardStepEdit
        : step >= kWizardStepCheck;
}

void PluginView::goToWizardStep(int step)
{
    if (step < 0 || step >= wizardStack_->count())
        return;
    // Never jump to a step outside the range the current app mode shows -
    // e.g. Next from Edit must not spill into Check while in Plugin
    // Creation mode, even if the underlying session data would allow it.
    if (!stepVisibleInCurrentMode(step))
        return;
    // Forward navigation (step-click or Next) is gated on prerequisites;
    // Back is always free since currentWizardStep_ is, by definition,
    // already reachable.
    if (step > currentWizardStep_ && step > maxReachableWizardStep())
        return;

    currentWizardStep_ = step;
    wizardStack_->setCurrentIndex(step);
    updateWizardStepStrip();
}

void PluginView::setStepStripMode(StepStripMode mode)
{
    stepStripMode_ = mode;
    for (int i = 0; i < wizardStepButtons_.size(); ++i)
        wizardStepButtons_[i]->setVisible(stepVisibleInCurrentMode(i));

    if (pageTitleLabel_ && pageSubtitleLabel_) {
        if (stepStripMode_ == StepStripMode::CheckBuild) {
            pageTitleLabel_->setText(tr("Check & Build"));
            pageSubtitleLabel_->setText(
                tr("Validate the plugin you're editing in Workspace, then build and install it."));
        } else {
            pageTitleLabel_->setText(tr("Workspace"));
            pageSubtitleLabel_->setText(
                tr("Select a plugin, edit CMD/TLM files, validate, build, and install."));
        }
    }

    // Re-clamps currentWizardStep_ into the new mode's range below - none of
    // currentPluginRoot_/currentComponentPath_/componentEditor_'s contents
    // are touched, so an open file's edits survive the mode switch.
    updateWizardStepStrip();
}

void PluginView::updateWizardStepStrip()
{
    const int maxReachable = maxReachableWizardStep();
    const int modeFloor = stepStripMode_ == StepStripMode::Creation ? kWizardStepPlugin : kWizardStepCheck;
    const int modeCeiling = stepStripMode_ == StepStripMode::Creation ? kWizardStepEdit : kWizardStepBuild;
    const int ceiling = maxReachable < modeCeiling ? maxReachable : modeCeiling;

    // A prerequisite met earlier can be lost later (e.g. the plugin
    // selection is cleared by a refresh that doesn't land on the same
    // plugin) while currentWizardStep_ still points deeper into the wizard.
    // Snap back rather than leaving the user stranded on a step whose page
    // now shows "no plugin/file selected" with no way forward.
    if (currentWizardStep_ > ceiling) {
        currentWizardStep_ = ceiling;
        wizardStack_->setCurrentIndex(currentWizardStep_);
    }
    // Symmetrically, never rest on a step below the current mode's own
    // range (e.g. arriving at Check & Build with no file open yet still
    // shows the Check page's own "nothing to check" empty state, rather
    // than falling back to a Plugin/File step whose button isn't even
    // visible in this mode).
    if (currentWizardStep_ < modeFloor) {
        currentWizardStep_ = modeFloor;
        wizardStack_->setCurrentIndex(currentWizardStep_);
    }

    if (currentWizardStep_ >= 0 && currentWizardStep_ < wizardStepButtons_.size())
        wizardStepButtons_[currentWizardStep_]->setChecked(true);

    wizardBackBtn_->setEnabled(currentWizardStep_ > modeFloor);
    wizardNextBtn_->setEnabled(currentWizardStep_ < modeCeiling
        && currentWizardStep_ < maxReachable);
    for (int i = 0; i < wizardStepButtons_.size(); ++i)
        wizardStepButtons_[i]->setEnabled(i <= maxReachable && stepVisibleInCurrentMode(i));

    updateWizardBreadcrumb();
}

void PluginView::updateWizardBreadcrumb()
{
    const QString plugin = selectedPluginName();
    if (plugin.isEmpty()) {
        breadcrumbLabel_->setText(tr("No plugin selected"));
        return;
    }
    breadcrumbLabel_->setText(currentComponentDisplayPath_.isEmpty()
        ? plugin
        : plugin + QString::fromUtf8(" › ") + currentComponentDisplayPath_);
}

void PluginView::bindViewModel()
{
    connect(refreshBtn_, &QPushButton::clicked, this, [this] {
        preRefreshPluginRoot_ = currentPluginRoot_;
        refreshingPluginList_ = true;
        vm_.refresh();
    });
    connect(&vm_, &ViewModels::PluginViewModel::pluginListChanged,
            this, &PluginView::restoreSelectionAfterRefresh);
    connect(installBtn_, &QPushButton::clicked, this, &PluginView::onInstallClicked);
    connect(removeBtn_, &QPushButton::clicked, this, &PluginView::onRemoveClicked);
    connect(removeAction_, &QAction::triggered, this, &PluginView::onRemoveClicked);
    connect(validateBtn_, &QPushButton::clicked, this, &PluginView::onValidateClicked);
    connect(buildBtn_, &QPushButton::clicked, this, &PluginView::onBuildClicked);
    connect(scaffoldBtn_, &QPushButton::clicked, this, &PluginView::onScaffoldClicked);
    connect(addTargetBtn_, &QPushButton::clicked, this, &PluginView::onAddTargetClicked);
    connect(addTargetAction_, &QAction::triggered, this, &PluginView::onAddTargetClicked);
    connect(addScriptAction_, &QAction::triggered, this, &PluginView::onAddScriptClicked);
    connect(openComponentBtn_, &QPushButton::clicked, this, &PluginView::onOpenComponentClicked);
    connect(browseGoBtn_, &QPushButton::clicked, this, &PluginView::onBrowseGoClicked);
    connect(browseList_, &QListWidget::itemDoubleClicked, this, &PluginView::onBrowseItemDoubleClicked);
    connect(saveComponentBtn_, &QPushButton::clicked, this, &PluginView::onSaveComponentClicked);
    connect(validateComponentBtn_, &QPushButton::clicked, this, &PluginView::onValidateComponentClicked);
    connect(validateOfflineBtn_, &QPushButton::clicked, this, &PluginView::onValidateOfflineClicked);
    connect(validateOfflineAction_, &QAction::triggered, this, &PluginView::onValidateOfflineClicked);
    connect(startCmdTlmEditBtn_, &QPushButton::clicked, this, &PluginView::onStartCmdTlmEditClicked);
    connect(insertCmdBtn_, &QPushButton::clicked, this, &PluginView::onInsertCmdClicked);
    connect(insertCmdAction_, &QAction::triggered, this, &PluginView::onInsertCmdClicked);
    connect(insertTlmBtn_, &QPushButton::clicked, this, &PluginView::onInsertTlmClicked);
    connect(insertTlmAction_, &QAction::triggered, this, &PluginView::onInsertTlmClicked);
    connect(insertScriptBtn_, &QPushButton::clicked, this, &PluginView::onInsertScriptClicked);
    connect(insertScriptAction_, &QAction::triggered, this, &PluginView::onInsertScriptClicked);
    connect(runScriptBtn_, &QPushButton::clicked, this, &PluginView::onRunScriptClicked);
    connect(checkScriptSyntaxBtn_, &QPushButton::clicked, this, &PluginView::onCheckScriptSyntaxClicked);
    connect(addFieldBtn_, &QPushButton::clicked, this, &PluginView::onAddFieldClicked);
    connect(addFieldAction_, &QAction::triggered, this, &PluginView::onAddFieldClicked);
    connect(addStructureFieldBtn_, &QPushButton::clicked, this, &PluginView::onAddStructureFieldClicked);
    connect(addStructureFieldAction_, &QAction::triggered, this, &PluginView::onAddStructureFieldClicked);
    connect(deleteStructureFieldBtn_, &QPushButton::clicked, this, &PluginView::onDeleteStructureFieldClicked);
    connect(deleteStructureFieldAction_, &QAction::triggered, this, &PluginView::onDeleteStructureFieldClicked);
    connect(refreshStructureBtn_, &QPushButton::clicked, this, &PluginView::onRefreshStructureClicked);
    connect(refreshStructureAction_, &QAction::triggered, this, &PluginView::onRefreshStructureClicked);
    connect(applyStructureBtn_, &QPushButton::clicked, this, &PluginView::onApplyStructureClicked);
    connect(applyStructureAction_, &QAction::triggered, this, &PluginView::onApplyStructureClicked);
    connect(applyBlockBtn_, &QPushButton::clicked, this, &PluginView::onApplyBlockClicked);
    connect(blockSelectorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PluginView::onBlockSelectionChanged);
    connect(applyManifestBlockBtn_, &QPushButton::clicked, this, &PluginView::onApplyManifestBlockClicked);
    connect(manifestBlockSelectorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PluginView::onManifestBlockSelectionChanged);
    connect(addManifestModifierAction_, &QAction::triggered, this, &PluginView::onAddManifestModifierClicked);
    connect(deleteManifestModifierAction_, &QAction::triggered, this, &PluginView::onDeleteManifestModifierClicked);
    connect(refreshManifestAction_, &QAction::triggered, this, &PluginView::refreshManifestTable);
    connect(manifestTable_, &QTableWidget::cellChanged, this, &PluginView::onManifestCellChanged);
    connect(componentEditorTabs_, &QTabWidget::currentChanged, this, [this](int index) {
        // Screen files have no structured "Apply" edit flow yet - Source-tab
        // text edits are the only way to change one - so refresh the preview
        // when the tab becomes current rather than re-parsing on every
        // keystroke while the user is still on the Source tab.
        if (index == 3)
            refreshScreenPreview();
    });
    connect(addScreenWidgetBtn_, &QPushButton::clicked, this, &PluginView::onAddScreenWidgetClicked);
    connect(toggleReferenceBtn_, &QPushButton::clicked, this, &PluginView::onToggleReferenceClicked);
    connect(toggleTerminalBtn_, &QPushButton::clicked, this, &PluginView::onToggleTerminalClicked);
    connect(diagnosticList_, &QListWidget::itemClicked, this, &PluginView::onDiagnosticItemClicked);
    connect(componentEditor_, &QTextEdit::textChanged,
            this, [this] {
                if (!currentComponentPath_.isEmpty())
                    setComponentDirty(true);
            });
    connect(structureTable_, &QTableWidget::itemSelectionChanged,
            this, [this] {
                deleteStructureFieldBtn_->setEnabled(isCmdTlmFile(currentComponentPath_)
                    && !structureTable_->selectedItems().isEmpty());
                const auto selected = structureTable_->selectionModel()->selectedRows();
                if (!selected.isEmpty())
                    focusEditorLineForStructureRow(selected.first().row());
                updateGroupedActionState();
            });
    connect(structureTable_, &QTableWidget::cellChanged,
            this, &PluginView::onStructureCellChanged);
    connect(structureTable_, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) {
                applyStructureRowToEditor(row);
            });

    connect(componentList_, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item) {
                if (!item) return;
                if (!(item->flags() & Qt::ItemIsSelectable)) return;
                componentPathLabel_->setText(tr("Selected: %1").arg(item->data(Qt::UserRole + 1).toString()));
                openComponentBtn_->setEnabled(true);
                setComponentHint(isCmdTlmFile(item->data(Qt::UserRole).toString())
                    ? tr("Open this CMD/TLM file to edit definitions, add fields, validate, and save.")
                    : tr("Open this plugin file to inspect or edit its text."));
                updateActionHints();
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
                    preRefreshPluginRoot_ = currentPluginRoot_;
                    refreshingPluginList_ = true;
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
                    preRefreshPluginRoot_ = currentPluginRoot_;
                    refreshingPluginList_ = true;
                    vm_.refresh();
                    const QString root = selectedPluginRoot();
                    if (!root.isEmpty())
                        cmdTlmVm_.listPluginFiles(root);
                }
            });

    connect(&infraVm_, &ViewModels::InfraViewModel::scriptAdded,
            this, [this](const QString&, bool success, const QString& detail) {
                detailTabs_->setCurrentIndex(0);
                detailEdit_->setPlainText(detail);
                if (success) {
                    preRefreshPluginRoot_ = currentPluginRoot_;
                    refreshingPluginList_ = true;
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
                updateActionHints();
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy() || cmdTlmVm_.isBusy();
                progressBar_->setVisible(busy);
                saveComponentBtn_->setEnabled(!busy && componentDirty_);
                openComponentBtn_->setEnabled(!busy && componentList_->currentItem() != nullptr);
                validateComponentBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                validateOfflineBtn_->setEnabled(!busy && !currentComponentPath_.isEmpty());
                startCmdTlmEditBtn_->setEnabled(!busy && !firstCmdTlmComponentPath_.isEmpty());
                addFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                addStructureFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                deleteStructureFieldBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_)
                    && !structureTable_->selectedItems().isEmpty());
                refreshStructureBtn_->setEnabled(!busy && isCmdTlmFile(currentComponentPath_));
                updateActionHints();
            });

    connect(&vm_, &ViewModels::PluginViewModel::statusMessageChanged,
            this, [this] {
                statusLabel_->setText(vm_.statusMessage());
                updateActionHints();
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::statusMessageChanged,
            this, [this] {
                if (!cmdTlmVm_.statusMessage().isEmpty())
                    statusLabel_->setText(cmdTlmVm_.statusMessage());
                updateActionHints();
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::connectionChanged,
            this, [this] {
                const bool connected = cmdTlmVm_.isConnected();
                browseGoBtn_->setEnabled(connected);
                browsePathEdit_->setEnabled(connected);
                if (connected)
                    browsePathEdit_->setText(cmdTlmVm_.defaultCmdTlmPath());
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
                            tr("[Build succeeded]\nGenerated gem file: %1\n\n--- Build log ---\n%2")
                                .arg(gem.isEmpty() ? tr("(path could not be determined)") : gem, summary));
                    } else {
                        detailEdit_->setPlainText(
                            tr("[Build failed] Check the log below for the cause.\n\n"
                            "--- Build log ---\n%1").arg(summary));
                    }
                    return;
                }
                detailEdit_->setPlainText(summary);
                // This only ever fires from the whole-plugin "Check" button
                // on the Plugin step's own toolbar, but its result renders
                // into detailEdit_ on the separate Build & Install step's
                // page - without this, a successful check left the user on
                // the unchanged Plugin step with no visible sign anything
                // had happened at all (failure already showed a warning
                // popup; success showed nothing).
                if (!valid)
                    QMessageBox::warning(this, tr("Workspace"), summary);
                else
                    QMessageBox::information(this, tr("Workspace"),
                        tr("Plugin check passed.\n\nSee full details in the "
                           "Overview tab (Build & Install step)."));
            });

    connect(&vm_, &ViewModels::PluginViewModel::actionCompleted,
            this, [this](const QString& name, bool ok) {
                if (pendingBuild_) {
                    pendingBuild_ = false;
                    if (ok)
                        QMessageBox::information(this, tr("Plugin Build"),
                            tr("Build complete.\nSee the generated gem path in the Overview tab."));
                    else
                        QMessageBox::warning(this, tr("Plugin Build"),
                            tr("Build failed.\nCheck the build log in the Overview tab for the cause."));
                    return;
                }
                if (!ok)
                    QMessageBox::warning(this, tr("Workspace"),
                        tr("Action failed for: %1").arg(name));
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::pluginFilesListed,
            this, &PluginView::populateComponentList);

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::directoryListed,
            this, [this](const QStringList& entries, const QString& path) {
                currentBrowseDir_ = path;
                browseList_->clear();
                for (const QString& e : entries)
                    browseList_->addItem(e);
                browseEmptyLabel_->setVisible(entries.isEmpty());
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileOpened,
            this, [this](const QString& path, const QString& content) {
                if (path != pendingOpenPath_)
                    return;

                currentComponentPath_ = path;
                currentComponentDisplayPath_ = path;
                if (currentComponentDisplayPath_.startsWith(currentPluginRoot_ + "/", Qt::CaseInsensitive))
                    currentComponentDisplayPath_ = currentComponentDisplayPath_.mid(currentPluginRoot_.size() + 1);
                componentPathLabel_->setToolTip(path);
                componentEditor_->setPlainText(content);
                setComponentDirty(false);
                const bool cmdTlm = isCmdTlmFile(path);
                const bool manifest = isPluginManifestFile(path);
                const bool screen = isScreenFile(path);
                const bool script = isScriptFile(path);
                validateComponentBtn_->setEnabled(cmdTlm);
                validateOfflineBtn_->setEnabled(true); // offline rules cover all config kinds
                insertCmdBtn_->setEnabled(cmdTlm);
                insertTlmBtn_->setEnabled(cmdTlm);
                insertScriptBtn_->setEnabled(script);
                runScriptBtn_->setEnabled(script);
                checkScriptSyntaxBtn_->setEnabled(script);
                addFieldBtn_->setEnabled(cmdTlm);
                addStructureFieldBtn_->setEnabled(cmdTlm);
                deleteStructureFieldBtn_->setEnabled(false);
                refreshStructureBtn_->setEnabled(cmdTlm);
                applyStructureBtn_->setEnabled(false);
                setComponentHint(cmdTlm
                    ? tr("Editing CMD/TLM. Add fields with the form, validate before saving, then build the plugin.")
                    : tr("Editing a plugin text file. Review changes carefully before saving."));
                componentDiagnostics_->setPlainText(
                    cmdTlm ? tr("Loaded CMD/TLM definition.") : tr("Loaded text file."));
                diagnosticList_->clear();
                diagnosticListEmptyLabel_->setVisible(true);
                refreshStructureTable();
                refreshManifestTable();
                refreshScreenPreview();
                detailTabs_->setCurrentIndex(1);
                setCmdTlmActionsVisible(cmdTlm);
                setManifestActionsVisible(manifest);
                setScreenPreviewActionsVisible(screen);
                if (componentEditorTabs_)
                    componentEditorTabs_->setCurrentIndex(0);

                // Wizard convenience: opening a file while on the File step
                // auto-advances to Edit (mirrors the Plugin->File auto-advance
                // in onTableSelectionChanged()).
                if (currentWizardStep_ == kWizardStepFile)
                    goToWizardStep(kWizardStepEdit);
                updateWizardStepStrip();
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileSaved,
            this, [this](const QString& path, bool success) {
                if (path != currentComponentPath_)
                    return;

                if (success)
                    setComponentDirty(false);
                componentDiagnostics_->setPlainText(
                    success ? tr("Saved: %1").arg(path) : tr("Save failed: %1").arg(path));
                if (detailTabs_)
                    detailTabs_->setCurrentIndex(1);
            });

    connect(&cmdTlmVm_, &ViewModels::CmdTlmViewModel::fileParsed,
            this, [this](const ViewModels::CmdTlmParseResult& result, const QString& filePath) {
                if (filePath != currentComponentPath_ || !isCmdTlmFile(filePath))
                    return;

                componentDiagnostics_->setPlainText(
                    QString("Blocks: %1, Errors: %2, Warnings: %3")
                        .arg(result.blocks.size())
                        .arg(result.errorCount())
                        .arg(result.warningCount()));

                diagnosticList_->clear();
                for (const auto& d : result.diagnostics) {
                    const bool isError = (d.severity == ViewModels::CmdTlmDiagnostic::Severity::Error);
                    const QString text = tr("%1 Line %2: %3")
                        .arg(isError ? tr("Error") : tr("Warning"))
                        .arg(d.line)
                        .arg(d.message);
                    auto* li = new QListWidgetItem(text, diagnosticList_);
                    li->setData(Qt::UserRole, d.line);
                    li->setForeground(isError ? QColor("#F44747") : QColor("#CCA700"));
                }
                diagnosticListEmptyLabel_->setVisible(diagnosticList_->count() == 0);

                if (detailTabs_)
                    detailTabs_->setCurrentIndex(1);
            });

    connect(&validatorVm_, &ViewModels::ValidatorViewModel::reportReady,
            this, [this] {
                if (!pendingOfflineValidation_)
                    return;
                pendingOfflineValidation_ = false;

                const auto& report = validatorVm_.report();
                componentDiagnostics_->setPlainText(tr("File check: %1   Source: %2")
                    .arg(report.summary(), currentComponentDisplayPath_.isEmpty()
                        ? validatorVm_.lastSource()
                        : currentComponentDisplayPath_));

                diagnosticList_->clear();
                for (const auto& d : report.diagnostics) {
                    const QString location = d.line > 0
                        ? tr("line %1").arg(d.line)
                        : tr("file");
                    QString text = QString("%1 %2: %3")
                        .arg(d.severityLabel(), location, d.message);
                    if (!d.rule.isEmpty())
                        text += QString(" (%1)").arg(d.rule);
                    if (!d.suggestion.isEmpty())
                        text += tr("  Fix: %1").arg(d.suggestion);

                    auto* li = new QListWidgetItem(text, diagnosticList_);
                    li->setData(Qt::UserRole, d.line);
                    using Severity = ViewModels::Validation::Diagnostic::Severity;
                    switch (d.severity) {
                    case Severity::Error:   li->setForeground(QColor("#F44747")); break;
                    case Severity::Warning: li->setForeground(QColor("#CCA700")); break;
                    case Severity::Info:    li->setForeground(QColor("#9A9A9A")); break;
                    }
                }
                diagnosticListEmptyLabel_->setVisible(diagnosticList_->count() == 0);

                if (detailTabs_)
                    detailTabs_->setCurrentIndex(1);
            });
}

void PluginView::onInstallClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Plugin Gem"), {}, tr("Gem files (*.gem);;All files (*)"));
    if (!path.isEmpty())
        vm_.install(path);
}

void PluginView::onRemoveClicked()
{
    const QString name = selectedPluginName();
    if (name.isEmpty()) return;
    if (QMessageBox::question(this, tr("Confirm Remove"),
            tr("Remove plugin: %1?").arg(name),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        vm_.remove(name);
}

void PluginView::onScaffoldClicked()
{
    // A successful creation refreshes the plugin list and resets the
    // currently open file (see scaffoldComplete below), which would
    // silently discard an unsaved edit with no warning - block it here the
    // same way onBuildClicked() already does for Build.
    if (componentDirty_) {
        QMessageBox::information(this, tr("New Plugin"),
            tr("Save the open CMD/TLM file before creating a new plugin."));
        return;
    }
    // No explicit refresh needed here: creating a plugin through this
    // dialog fires infraVm_'s scaffoldComplete signal (connected above),
    // which already snapshots/refreshes/reselects for us. Doing it again
    // here would double-fire and stomp the snapshot that connection just took.
    Dialogs::PluginWizard wizard(infraVm_, this);
    wizard.exec();
}

void PluginView::onAddTargetClicked()
{
    const QString root = selectedPluginRoot();
    if (root.isEmpty()) {
        QMessageBox::information(this, tr("Add Target"),
            tr("Select a plugin folder before adding a target."));
        return;
    }
    // Same unsaved-edit data-loss risk as onScaffoldClicked() above: a
    // successful target-add refreshes the file list and resets the open file.
    if (componentDirty_) {
        QMessageBox::information(this, tr("Add Target"),
            tr("Save the open CMD/TLM file before adding a target."));
        return;
    }
    // No explicit refresh needed here: adding a target fires infraVm_'s
    // targetAdded signal (connected above), which already
    // snapshots/refreshes/reselects and reloads the file list for us.
    Dialogs::AddTargetDialog dlg(infraVm_, root, this);
    dlg.exec();
}

void PluginView::onAddScriptClicked()
{
    const QString root = selectedPluginRoot();
    if (root.isEmpty()) {
        QMessageBox::information(this, tr("Add Script"),
            tr("Select a plugin folder before adding a script."));
        return;
    }
    if (componentDirty_) {
        QMessageBox::information(this, tr("Add Script"),
            tr("Save the open file before adding a script."));
        return;
    }

    // Derive the known target names from two sources, since either may be
    // unavailable depending on what the user has opened so far: the
    // plugin.txt TARGET blocks already parsed into currentManifestBlocks_
    // (if plugin.txt has been opened this session, same source
    // onNewManifestInterfaceOrRouter() uses for its MAP_TARGET combo), and
    // the plugin's own file list ("targets/<NAME>/...", the directory
    // layout PluginTemplateEngine actually generates).
    QSet<QString> targetSet;
    for (const auto& block : currentManifestBlocks_)
        if (block.kind == ViewModels::PluginManifestBlock::Kind::Target && !block.name.isEmpty())
            targetSet.insert(block.name);
    static const QRegularExpression targetDirPattern("^targets/([^/]+)/");
    for (int i = 0; i < componentList_->count(); ++i) {
        const QString relPath = componentList_->item(i)->data(Qt::UserRole + 1).toString();
        const auto match = targetDirPattern.match(relPath);
        if (match.hasMatch())
            targetSet.insert(match.captured(1));
    }
    QStringList knownTargets(targetSet.constBegin(), targetSet.constEnd());
    knownTargets.sort(Qt::CaseInsensitive);

    // Neither source above is guaranteed (plugin.txt may not have been
    // opened yet this session, and not every plugin nests cmd_tlm/screens/
    // procedures under targets/<NAME>/) - the dialog's target combo is
    // editable either way, so an empty list just means the user types the
    // target name themselves instead of picking one, same as AddTargetDialog.
    Dialogs::NewScriptDialog dlg(infraVm_, root, knownTargets, this);
    dlg.exec();
}

void PluginView::onValidateClicked()
{
    const QString root = selectedPluginRoot();
    if (!root.isEmpty()) {
        vm_.validate(root);
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Plugin Directory or Gem"), {}, tr("All files (*)"));
    if (!path.isEmpty()) vm_.validate(path);
}

void PluginView::onBuildClicked()
{
    if (componentDirty_) {
        QMessageBox::information(this, tr("Build Plugin"),
            tr("Save the open CMD/TLM file before building the plugin."));
        return;
    }

    const QString root = selectedPluginRoot();
    if (root.isEmpty()) {
        QMessageBox::information(this, tr("Build Plugin"),
            tr("Select a plugin folder first."));
        return;
    }
    pendingBuild_ = true;
    detailTabs_->setCurrentIndex(0);
    detailEdit_->setPlainText(tr("Building plugin...\n%1").arg(root));
    vm_.build(root);
}

void PluginView::onTableSelectionChanged()
{
    const bool hasSel = tableView_->selectionModel()->hasSelection();

    // Refreshing the plugin list (Refresh button, New Plugin, Add Target)
    // resets the table model. Qt's model-reset-driven selection clear does
    // not reliably emit selectionChanged (confirmed empirically), so the
    // "!hasSel" transient case below is mostly a defensive no-op; the real
    // work happens when restoreSelectionAfterRefresh() (on pluginListChanged)
    // explicitly reselects a row, which *does* fire this slot with hasSel
    // true. Compare against preRefreshPluginRoot_ (captured before this
    // refresh cycle, and before scaffoldComplete-style handlers reassign
    // currentPluginRoot_ to a new plugin) rather than currentPluginRoot_
    // itself, so a routine "same plugin" refresh preserves the in-progress
    // file-editing session while a genuine switch to a different plugin
    // still gets the normal reset below.
    if (refreshingPluginList_) {
        if (!hasSel)
            return; // wait for restoreSelectionAfterRefresh() to reselect
        const auto* reselected =
            vm_.pluginModel()->pluginAt(tableView_->selectionModel()->selectedRows().first().row());
        const QString reselectedRoot = reselected ? QString::fromStdString(reselected->rootPath) : QString();
        refreshingPluginList_ = false;
        if (!preRefreshPluginRoot_.isEmpty() && reselectedRoot == preRefreshPluginRoot_) {
            preRefreshPluginRoot_.clear();
            updateWizardStepStrip();
            return;
        }
        preRefreshPluginRoot_.clear();
        // Otherwise this is a genuine switch to a different plugin - fall
        // through to the normal reset below.
    }

    // Wizard convenience: selecting a plugin while on the Plugin step
    // auto-advances to File - the Next button remains available too, and
    // this doesn't fire when the user is elsewhere (e.g. re-selecting a
    // plugin from the Edit step shouldn't yank them back to File).
    if (hasSel && currentWizardStep_ == kWizardStepPlugin)
        goToWizardStep(kWizardStepFile);

    removeBtn_->setEnabled(hasSel && !vm_.isBusy());
    buildBtn_->setEnabled(hasSel && !vm_.isBusy());
    addTargetBtn_->setEnabled(hasSel && !infraVm_.isBusy());

    componentList_->clear();
    updateComponentEmptyState();
    componentEditor_->clear();
    componentDiagnostics_->clear();
    diagnosticList_->clear();
    diagnosticListEmptyLabel_->setVisible(true);
    structureTable_->setRowCount(0);
    currentBlocks_.clear();
    refreshBlockEditor();
    currentPluginRoot_.clear();
    currentComponentPath_.clear();
    currentComponentDisplayPath_.clear();
    firstCmdTlmComponentPath_.clear();
    componentDirty_ = false;
    componentPathLabel_->setText(hasSel ? tr("Loading plugin files") : tr("Select a plugin file"));
    setComponentHint(hasSel
        ? tr("Loading plugin files. CMD/TLM files will appear grouped first.")
        : tr("Select a plugin first."));
    saveComponentBtn_->setEnabled(false);
    openComponentBtn_->setEnabled(false);
    validateComponentBtn_->setEnabled(false);
    validateOfflineBtn_->setEnabled(false);
    startCmdTlmEditBtn_->setEnabled(false);
    insertCmdBtn_->setEnabled(false);
    insertTlmBtn_->setEnabled(false);
    insertScriptBtn_->setEnabled(false);
    runScriptBtn_->setEnabled(false);
    checkScriptSyntaxBtn_->setEnabled(false);
    addFieldBtn_->setEnabled(false);
    addStructureFieldBtn_->setEnabled(false);
    deleteStructureFieldBtn_->setEnabled(false);
    refreshStructureBtn_->setEnabled(false);
    applyStructureBtn_->setEnabled(false);
    updateActionHints();
    setCmdTlmActionsVisible(false);
    startCmdTlmEditBtn_->setVisible(false);

    if (!hasSel) {
        detailEdit_->clear();
        pluginSummaryLabel_->setText(tr("Select a plugin folder to inspect and edit its files."));
        setComponentHint(tr("Select a plugin first."));
        updateWizardStepStrip();
        return;
    }

    const auto* plugin =
        vm_.pluginModel()->pluginAt(tableView_->selectionModel()->selectedRows().first().row());
    if (!plugin) return;

    const QString pluginName = QString::fromStdString(plugin->name);
    const QString pluginRoot = QString::fromStdString(plugin->rootPath);
    const int targetCount = static_cast<int>(plugin->targets.size());
    pluginSummaryLabel_->setText(tr("%1  |  %2 target(s)  |  %3")
        .arg(pluginName)
        .arg(targetCount)
        .arg(pluginRoot));

    const QString detail = tr(
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
        .arg(plugin->isInstalled() ? tr("Ready") : tr("Needs attention"))
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
            return targets.isEmpty() ? tr("  (none)") : targets.join('\n');
        }());

    detailEdit_->setPlainText(detail);
    currentPluginRoot_ = selectedPluginRoot();
    if (!currentPluginRoot_.isEmpty())
        cmdTlmVm_.listPluginFiles(currentPluginRoot_);
    updateWizardStepStrip();
}

void PluginView::restoreSelectionAfterRefresh()
{
    if (!refreshingPluginList_)
        return; // pluginListChanged fired for a reason other than our own refresh

    if (currentPluginRoot_.isEmpty()) {
        refreshingPluginList_ = false;
        return;
    }

    auto* model = vm_.pluginModel();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto* plugin = model->pluginAt(row);
        if (plugin && QString::fromStdString(plugin->rootPath) == currentPluginRoot_) {
            tableView_->selectRow(row); // fires onTableSelectionChanged(), which clears the flag
            return;
        }
    }

    // The plugin no longer exists (e.g. removed elsewhere) - run the real
    // reset now for the resulting empty selection.
    refreshingPluginList_ = false;
    onTableSelectionChanged();
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
        componentDiagnostics_->setPlainText(tr("CMD/TLM validation is available for cmd_tlm files."));
        if (detailTabs_)
            detailTabs_->setCurrentIndex(1);
        return;
    }
    cmdTlmVm_.parseContent(componentEditor_->toPlainText(), currentComponentPath_);
    refreshStructureTable();
}

void PluginView::onValidateOfflineClicked()
{
    if (currentComponentPath_.isEmpty()) return;
    const QString content = componentEditor_->toPlainText();
    if (content.trimmed().isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Nothing to validate."));
        if (detailTabs_)
            detailTabs_->setCurrentIndex(1);
        return;
    }

    pendingOfflineValidation_ = true;
    componentDiagnostics_->setPlainText(tr("Checking file..."));
    if (detailTabs_)
        detailTabs_->setCurrentIndex(1);
    validatorVm_.checkContent(content);
}

void PluginView::onStartCmdTlmEditClicked()
{
    if (firstCmdTlmComponentPath_.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("No CMD/TLM definition file was found in this plugin."));
        return;
    }
    if (!confirmDiscardUnsavedChanges())
        return;

    detailTabs_->setCurrentIndex(1);
    cmdTlmVm_.openFile(firstCmdTlmComponentPath_);
}

void PluginView::onInsertCmdClicked()
{
    insertTextAtCursor(Widgets::CmdTlmSnippets::command());
    refreshStructureTable();
}

void PluginView::onInsertTlmClicked()
{
    insertTextAtCursor(Widgets::CmdTlmSnippets::telemetry());
    refreshStructureTable();
}

void PluginView::onInsertScriptClicked()
{
    insertTextAtCursor(Widgets::CmdTlmSnippets::scriptStep());
}

void PluginView::onRunScriptClicked()
{
    if (!isScriptFile(currentComponentPath_))
        return;
    if (!logViewerVm_.isConnected()) {
        QMessageBox::information(this, tr("Run Script"),
            tr("Connect to an OpenC3 environment first."));
        return;
    }
    if (componentDirty_) {
        QMessageBox::information(this, tr("Run Script"),
            tr("Save the script before running it."));
        return;
    }

    // Runs directly on the connected host via `ruby` (same execution model
    // PluginService::build() already uses for `gem build`) - not the real
    // COSMOS Script Runner, so cmd()/tlm() only resolve if the openc3 script
    // libraries happen to be loadable in that environment. Honest, scoped
    // "run this file" support, not a faithful Script Runner replacement.
    const std::string cmd =
        "cd " + Core::Connection::shellQuote(currentPluginRoot_.toStdString())
        + " && ruby " + Core::Connection::shellQuote(currentComponentDisplayPath_.toStdString())
        + " 2>&1";

    if (terminalPanel_ && !terminalPanel_->isVisible())
        onToggleTerminalClicked();

    logViewerVm_.startStream(QString::fromStdString(cmd));
    componentDiagnostics_->setPlainText(tr("Running script - see Terminal panel for output."));
}

void PluginView::onCheckScriptSyntaxClicked()
{
    if (!isScriptFile(currentComponentPath_))
        return;
    if (!logViewerVm_.isConnected()) {
        QMessageBox::information(this, tr("Check Syntax"),
            tr("Connect to an OpenC3 environment first."));
        return;
    }
    if (componentDirty_) {
        QMessageBox::information(this, tr("Check Syntax"),
            tr("Save the script before checking its syntax."));
        return;
    }

    // `ruby -c` only parses the file (no execution, no openc3 gem needed) -
    // a lightweight, always-available complement to Run Script, catching
    // typos/syntax errors even without a full COSMOS runtime.
    const std::string cmd =
        "cd " + Core::Connection::shellQuote(currentPluginRoot_.toStdString())
        + " && ruby -c " + Core::Connection::shellQuote(currentComponentDisplayPath_.toStdString())
        + " 2>&1";

    if (terminalPanel_ && !terminalPanel_->isVisible())
        onToggleTerminalClicked();

    logViewerVm_.startStream(QString::fromStdString(cmd));
    componentDiagnostics_->setPlainText(tr("Checking script syntax - see Terminal panel for output."));
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
        componentDiagnostics_->setPlainText(tr("Select a field row first."));
        return;
    }

    const auto* lineCell = structureTable_->item(selected.first().row(), 0);
    if (!lineCell)
        return;

    const int lineNumber = lineCell->data(Qt::UserRole).toInt();
    const QString fieldName = structureTable_->item(selected.first().row(), 2)
        ? structureTable_->item(selected.first().row(), 2)->text()
        : tr("field");
    const auto answer = QMessageBox::question(
        this,
        tr("Delete Field"),
        tr("Delete field '%1' from this CMD/TLM file?").arg(fieldName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    // Sub-directive lines (STATE, UNITS, ...) modify the field they
    // immediately follow - deleting only the field's own line left them
    // orphaned under whatever field ended up above once the parent was
    // gone, silently altering that unrelated field instead of failing
    // loudly (this app's own parser has no notion of "orphaned", so it
    // reported no diagnostic for the corruption either).
    int linesToDelete = 1;
    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber);
    while (block.isValid()) {
        const QString trimmed = block.text().trimmed();
        if (trimmed.isEmpty())
            break;
        const QString firstTok = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(0);
        if (!ViewModels::CmdTlmParser::isSubDirectiveKeyword(firstTok))
            break;
        ++linesToDelete;
        block = block.next();
    }

    for (int i = 0; i < linesToDelete; ++i)
        deleteEditorLine(lineNumber);
    refreshStructureTable();
    componentDiagnostics_->setPlainText(tr("Deleted field '%1' (%2 line(s)). Save the file to keep it.")
        .arg(fieldName).arg(linesToDelete));
}

void PluginView::onRefreshStructureClicked()
{
    refreshStructureTable();
}

void PluginView::onApplyStructureClicked()
{
    const auto selected = structureTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Select a field row first."));
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
    updateComponentEmptyState();

    componentPathLabel_->setText(files.isEmpty()
        ? tr("No plugin component files found")
        : tr("Select a plugin file"));
    setComponentHint(files.isEmpty()
        ? tr("No editable plugin files were found. Check that plugin.txt, gemspec, and targets exist.")
        : tr("Found %1 file(s), including %2 CMD/TLM definition file(s). Use Edit CMD/TLM for the usual flow.")
              .arg(files.size()).arg(cmdTlmCount));
    startCmdTlmEditBtn_->setEnabled(cmdTlmCount > 0 && !cmdTlmVm_.isBusy());
    startCmdTlmEditBtn_->setVisible(cmdTlmCount > 0);
    updateActionHints();
}

void PluginView::setComponentHint(const QString& text)
{
    if (componentHintLabel_)
        componentHintLabel_->setText(text);
}

void PluginView::updateActionHints()
{
    const bool hasPlugin = tableView_->selectionModel()->hasSelection();
    const bool hasSelectedFile = componentList_->currentItem() != nullptr
        && (componentList_->currentItem()->flags() & Qt::ItemIsSelectable);
    const bool hasOpenFile = !currentComponentPath_.isEmpty();
    const bool cmdTlm = isCmdTlmFile(currentComponentPath_);
    const bool busy = vm_.isBusy() || cmdTlmVm_.isBusy();
    const QString connectReason = tr("Connect to an OpenC3 environment first.");
    const QString selectPluginReason = tr("Select a plugin folder first.");
    const QString fileReason = tr("Open a CMD/TLM .txt file first.");
    const QString openPluginFileReason = tr("Open a plugin file first.");
    const QString busyReason = tr("Wait for the current plugin operation to finish.");

    refreshBtn_->setToolTip(connectReason);
    // Add Target/Build/Remove/Check are only ever visible once a plugin is
    // selected (see updateGroupedActionState()), so their tooltip only needs
    // the short, unconditional purpose - "select a plugin first" is already
    // covered by the inline hint below instead of being repeated here.
    addTargetBtn_->setToolTip(tr("Add a target folder structure to this plugin."));
    buildBtn_->setToolTip(busy ? busyReason : tr("Build this plugin into a gem."));
    removeBtn_->setToolTip(busy ? busyReason : tr("Remove this plugin from OpenC3."));
    validateBtn_->setToolTip(tr("Check this plugin with openc3cli."));
    openComponentBtn_->setToolTip(!hasSelectedFile ? tr("Select a plugin file first.") : tr("Open the selected plugin file."));
    startCmdTlmEditBtn_->setToolTip(firstCmdTlmComponentPath_.isEmpty() ? fileReason : tr("Open the first CMD/TLM definition and start editing."));
    saveComponentBtn_->setToolTip(!hasOpenFile ? openPluginFileReason : tr("Save the open plugin file."));
    validateComponentBtn_->setToolTip(!cmdTlm ? fileReason : tr("Validate the open CMD/TLM .txt file."));
    validateOfflineBtn_->setToolTip(!hasOpenFile ? openPluginFileReason : tr("Run the full per-rule offline validator on this file."));
    insertCmdBtn_->setToolTip(!cmdTlm ? fileReason : tr("Insert a COMMAND template."));
    insertTlmBtn_->setToolTip(!cmdTlm ? fileReason : tr("Insert a TELEMETRY template."));
    insertScriptBtn_->setToolTip(!isScriptFile(currentComponentPath_)
        ? tr("Open a procedures/*.rb script file first.")
        : tr("Insert a cmd()/wait_check() script step."));
    addFieldBtn_->setToolTip(!cmdTlm ? fileReason : tr("Add a CMD/TLM field at the cursor."));
    addStructureFieldBtn_->setToolTip(!cmdTlm ? fileReason : tr("Add a CMD/TLM structure row."));
    deleteStructureFieldBtn_->setToolTip(structureTable_->selectedItems().isEmpty()
        ? tr("Select a structure row first.")
        : tr("Delete the selected structure row."));
    refreshStructureBtn_->setToolTip(!cmdTlm ? fileReason : tr("Refresh the structure editor from the source text."));
    applyStructureBtn_->setToolTip(!cmdTlm ? fileReason : tr("Apply the selected structure row to the source text."));

    if (!hasPlugin)
        statusLabel_->setText(tr("Select a plugin to check, build, or install."));
    else if (!hasOpenFile)
        statusLabel_->setText(tr("Open a file, then edit and save. Advanced actions are in Check, Insert, and Fields."));

    updateGroupedActionState();
}

void PluginView::updateComponentEmptyState()
{
    if (componentListEmptyLabel_)
        componentListEmptyLabel_->setVisible(componentList_->count() == 0);
}

void PluginView::openSelectedComponent(QListWidgetItem* item)
{
    if (!item) return;
    if (!(item->flags() & Qt::ItemIsSelectable)) return;
    if (!confirmDiscardUnsavedChanges())
        return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    pendingOpenPath_ = path;
    cmdTlmVm_.openFile(path);
}

void PluginView::openBrowsePath(const QString& remotePath)
{
    if (remotePath.isEmpty()) return;
    if (!confirmDiscardUnsavedChanges())
        return;
    pendingOpenPath_ = remotePath;
    cmdTlmVm_.openFile(remotePath);
}

void PluginView::onBrowseGoClicked()
{
    cmdTlmVm_.listDirectory(browsePathEdit_->text().trimmed());
}

void PluginView::onBrowseItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString name = item->text();
    const QString path = (currentBrowseDir_.isEmpty() || currentBrowseDir_.endsWith('/'))
                         ? currentBrowseDir_ + name
                         : currentBrowseDir_ + '/' + name;

    // Heuristic: no extension or trailing slash -> directory (matches the
    // same convention the retired standalone CMD/TLM browser used).
    if (!name.contains('.') || name.endsWith('/')) {
        browsePathEdit_->setText(path.endsWith('/') ? path.chopped(1) : path);
        cmdTlmVm_.listDirectory(browsePathEdit_->text());
    } else {
        openBrowsePath(path);
    }
}

void PluginView::insertTextAtCursor(const QString& text)
{
    auto cursor = componentEditor_->textCursor();
    cursor.insertText(text);
    componentEditor_->setTextCursor(cursor);
}

void PluginView::refreshStructureTable()
{
    updatingStructureTable_ = true;
    const QSignalBlocker blocker(structureTable_);

    structureTable_->setRowCount(0);
    applyStructureBtn_->setEnabled(false);

    if (!isCmdTlmFile(currentComponentPath_)) {
        currentBlocks_.clear();
        refreshBlockEditor();
        updatingStructureTable_ = false;
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
            lineItem->setData(Qt::UserRole + 3, item.keyword);
            lineItem->setData(Qt::UserRole + 4, item.hasExplicitOffset);
            lineItem->setData(Qt::UserRole + 5, item.isArray);

            auto* blockItem = new QTableWidgetItem(blockLabel);
            blockItem->setFlags(blockItem->flags() & ~Qt::ItemIsEditable);
            blockItem->setToolTip(blockLabel);

            structureTable_->setItem(row, 0, lineItem);
            structureTable_->setItem(row, 1, blockItem);
            structureTable_->setItem(row, 2, new QTableWidgetItem(item.name));
            structureTable_->setItem(row, 3, new QTableWidgetItem(item.hasExplicitOffset
                ? QString::number(item.bitOffset)
                : QString{}));
            structureTable_->setItem(row, 4, new QTableWidgetItem(QString::number(item.bitSize)));
            structureTable_->setItem(row, 5, new QTableWidgetItem(item.dataType));
            structureTable_->setItem(row, 6, new QTableWidgetItem(item.isArray
                ? QString::number(item.arrayBitSize)
                : QString{}));
            structureTable_->setItem(row, 7, new QTableWidgetItem(item.minVal));
            structureTable_->setItem(row, 8, new QTableWidgetItem(item.maxVal));
            structureTable_->setItem(row, 9, new QTableWidgetItem(item.defaultVal));
            structureTable_->setItem(row, 10, new QTableWidgetItem(item.description));
            ++row;
        }
    }

    componentDiagnostics_->setPlainText(tr(
        "Structure loaded: %1 block(s), %2 field(s), %3 error(s), %4 warning(s).")
        .arg(result.blocks.size())
        .arg(structureTable_->rowCount())
        .arg(result.errorCount())
        .arg(result.warningCount()));
    updatingStructureTable_ = false;
}

void PluginView::refreshManifestTable()
{
    updatingManifestTable_ = true;
    const QSignalBlocker blocker(manifestTable_);
    manifestTable_->setRowCount(0);

    if (!isPluginManifestFile(currentComponentPath_)) {
        currentManifestBlocks_.clear();
        refreshManifestBlockEditor();
        updatingManifestTable_ = false;
        return;
    }

    static const QMap<ViewModels::PluginManifestBlock::Kind, QString> kKindLabels = {
        {ViewModels::PluginManifestBlock::Kind::Target,       "TARGET"},
        {ViewModels::PluginManifestBlock::Kind::Interface,    "INTERFACE"},
        {ViewModels::PluginManifestBlock::Kind::Router,       "ROUTER"},
        {ViewModels::PluginManifestBlock::Kind::Microservice, "MICROSERVICE"},
        {ViewModels::PluginManifestBlock::Kind::Tool,         "TOOL"},
        {ViewModels::PluginManifestBlock::Kind::Widget,       "WIDGET"},
        {ViewModels::PluginManifestBlock::Kind::Variable,     "VARIABLE"},
    };

    const auto result = ViewModels::PluginManifestParser::parse(componentEditor_->toPlainText());
    currentManifestBlocks_ = result.blocks;
    refreshManifestBlockEditor();

    int row = 0;
    for (const auto& block : result.blocks) {
        const QString kindLabel = kKindLabels.value(block.kind);

        manifestTable_->insertRow(row);
        auto* lineItem = new QTableWidgetItem(QString::number(block.lineNumber));
        lineItem->setFlags(lineItem->flags() & ~Qt::ItemIsEditable);
        lineItem->setData(Qt::UserRole, block.lineNumber);
        lineItem->setData(Qt::UserRole + 1, false); // not a modifier row
        manifestTable_->setItem(row, 0, lineItem);

        const QString blockLabel = block.name.isEmpty()
            ? kindLabel : QString("%1 %2").arg(kindLabel, block.name);
        auto* blockItem = new QTableWidgetItem(blockLabel);
        blockItem->setFlags(blockItem->flags() & ~Qt::ItemIsEditable);
        manifestTable_->setItem(row, 1, blockItem);

        QStringList blockArgs;
        if (!block.classFileOrFolder.isEmpty()) blockArgs << block.classFileOrFolder;
        blockArgs << block.extraArgs;
        auto* argsItem = new QTableWidgetItem(blockArgs.join(' '));
        argsItem->setFlags(argsItem->flags() & ~Qt::ItemIsEditable);
        manifestTable_->setItem(row, 2, argsItem);

        auto* kindItem = new QTableWidgetItem(kindLabel);
        kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
        manifestTable_->setItem(row, 3, kindItem);
        ++row;

        for (const auto& mod : block.modifiers) {
            manifestTable_->insertRow(row);
            auto* modLineItem = new QTableWidgetItem(QString::number(mod.lineNumber));
            modLineItem->setFlags(modLineItem->flags() & ~Qt::ItemIsEditable);
            modLineItem->setData(Qt::UserRole, mod.lineNumber);
            modLineItem->setData(Qt::UserRole + 1, true); // modifier row
            manifestTable_->setItem(row, 0, modLineItem);

            // Keyword/Args cells stay editable (default QTableWidgetItem
            // flags) so onManifestCellChanged() can apply in-place edits -
            // unlike every other cell in this table, which is read-only.
            auto* modKeywordItem = new QTableWidgetItem("  " + mod.keyword);
            manifestTable_->setItem(row, 1, modKeywordItem);

            auto* modArgsItem = new QTableWidgetItem(mod.rawArgsText);
            manifestTable_->setItem(row, 2, modArgsItem);

            auto* modKindItem = new QTableWidgetItem(tr("modifier"));
            modKindItem->setFlags(modKindItem->flags() & ~Qt::ItemIsEditable);
            manifestTable_->setItem(row, 3, modKindItem);
            ++row;
        }
    }
    updatingManifestTable_ = false;
}

void PluginView::refreshManifestBlockEditor()
{
    const bool editable = isPluginManifestFile(currentComponentPath_) && !currentManifestBlocks_.isEmpty();

    QSignalBlocker blocker(manifestBlockSelectorCombo_);
    manifestBlockSelectorCombo_->clear();
    for (const auto& block : currentManifestBlocks_) {
        const QString kindLabel = [&] {
            switch (block.kind) {
            case ViewModels::PluginManifestBlock::Kind::Target:       return "TARGET";
            case ViewModels::PluginManifestBlock::Kind::Interface:    return "INTERFACE";
            case ViewModels::PluginManifestBlock::Kind::Router:       return "ROUTER";
            case ViewModels::PluginManifestBlock::Kind::Microservice: return "MICROSERVICE";
            case ViewModels::PluginManifestBlock::Kind::Tool:         return "TOOL";
            case ViewModels::PluginManifestBlock::Kind::Widget:       return "WIDGET";
            case ViewModels::PluginManifestBlock::Kind::Variable:     return "VARIABLE";
            }
            return "";
        }();
        manifestBlockSelectorCombo_->addItem(block.name.isEmpty()
            ? QString("%1  (line %2)").arg(kindLabel).arg(block.lineNumber)
            : QString("%1 %2  (line %3)").arg(kindLabel, block.name).arg(block.lineNumber));
    }

    manifestBlockSelectorCombo_->setEnabled(editable);
    manifestNameEdit_->setEnabled(editable);
    manifestClassOrFolderEdit_->setEnabled(editable);
    manifestArgsEdit_->setEnabled(editable);
    applyManifestBlockBtn_->setEnabled(editable);

    if (editable) {
        manifestBlockSelectorCombo_->setCurrentIndex(0);
        populateManifestBlockForm(0);
    } else {
        manifestKindLabel_->clear();
        manifestNameEdit_->clear();
        manifestClassOrFolderEdit_->clear();
        manifestArgsEdit_->clear();
    }
}

void PluginView::populateManifestBlockForm(int blockIndex)
{
    if (blockIndex < 0 || blockIndex >= currentManifestBlocks_.size())
        return;

    const auto& block = currentManifestBlocks_[blockIndex];
    const QString kindLabel = [&] {
        switch (block.kind) {
        case ViewModels::PluginManifestBlock::Kind::Target:       return "TARGET";
        case ViewModels::PluginManifestBlock::Kind::Interface:    return "INTERFACE";
        case ViewModels::PluginManifestBlock::Kind::Router:       return "ROUTER";
        case ViewModels::PluginManifestBlock::Kind::Microservice: return "MICROSERVICE";
        case ViewModels::PluginManifestBlock::Kind::Tool:         return "TOOL";
        case ViewModels::PluginManifestBlock::Kind::Widget:       return "WIDGET";
        case ViewModels::PluginManifestBlock::Kind::Variable:     return "VARIABLE";
        }
        return "";
    }();
    manifestKindLabel_->setText(kindLabel);

    manifestNameEdit_->setText(block.name);
    manifestNameEdit_->setCursorPosition(0);
    manifestClassOrFolderEdit_->setText(block.classFileOrFolder);
    manifestClassOrFolderEdit_->setCursorPosition(0);
    manifestArgsEdit_->setText(block.extraArgs.join(' '));
    manifestArgsEdit_->setCursorPosition(0);

    // TOOL/WIDGET carry no fixed Name/Folder-Class positional args (pure
    // modifier bags) - disable those two fields for them so Apply Block
    // can't accidentally inject a stray leading token onto the header line.
    const bool hasNameAndClass = block.kind != ViewModels::PluginManifestBlock::Kind::Tool
                               && block.kind != ViewModels::PluginManifestBlock::Kind::Widget;
    manifestNameEdit_->setEnabled(hasNameAndClass);
    manifestClassOrFolderEdit_->setEnabled(
        hasNameAndClass && block.kind != ViewModels::PluginManifestBlock::Kind::Variable);
}

void PluginView::onManifestBlockSelectionChanged(int index)
{
    populateManifestBlockForm(index);
    const auto selected = (index >= 0 && index < currentManifestBlocks_.size())
        ? currentManifestBlocks_[index].lineNumber : 0;
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

void PluginView::onApplyManifestBlockClicked()
{
    const int index = manifestBlockSelectorCombo_->currentIndex();
    if (index < 0 || index >= currentManifestBlocks_.size()) {
        componentDiagnostics_->setPlainText(tr("Select a manifest block first."));
        return;
    }

    const auto& block = currentManifestBlocks_[index];
    const QString name   = manifestNameEdit_->text().trimmed().toUpper();
    const QString folder = manifestClassOrFolderEdit_->text().trimmed();
    const QString args   = manifestArgsEdit_->text().trimmed();

    QString line;
    switch (block.kind) {
    case ViewModels::PluginManifestBlock::Kind::Target:
        if (folder.isEmpty() || name.isEmpty()) {
            componentDiagnostics_->setPlainText(tr("Folder/Class and Name are required for TARGET."));
            return;
        }
        line = QString("TARGET %1 %2").arg(folder, name);
        break;
    case ViewModels::PluginManifestBlock::Kind::Interface:
    case ViewModels::PluginManifestBlock::Kind::Router:
        if (name.isEmpty() || folder.isEmpty()) {
            componentDiagnostics_->setPlainText(tr("Name and Folder/Class are required."));
            return;
        }
        line = QString("%1 %2 %3")
            .arg(block.kind == ViewModels::PluginManifestBlock::Kind::Interface ? "INTERFACE" : "ROUTER",
                 name, folder);
        break;
    case ViewModels::PluginManifestBlock::Kind::Microservice:
        if (folder.isEmpty() || name.isEmpty()) {
            componentDiagnostics_->setPlainText(tr("Folder/Class and Name are required for MICROSERVICE."));
            return;
        }
        line = QString("MICROSERVICE %1 %2").arg(folder, name);
        break;
    case ViewModels::PluginManifestBlock::Kind::Tool:
        line = "TOOL";
        break;
    case ViewModels::PluginManifestBlock::Kind::Widget:
        line = "WIDGET";
        break;
    case ViewModels::PluginManifestBlock::Kind::Variable:
        if (name.isEmpty()) {
            componentDiagnostics_->setPlainText(tr("Name is required for VARIABLE."));
            return;
        }
        line = QString("VARIABLE %1").arg(name);
        break;
    }
    if (!args.isEmpty())
        line += " " + args;

    replaceEditorLine(block.lineNumber, line);
    componentDiagnostics_->setPlainText(tr(
        "Updated block at line %1. Modifier lines were preserved. Save to keep it.")
        .arg(block.lineNumber));
    refreshManifestTable();
}

void PluginView::insertManifestModifierAfterBlock(int blockIndex, const QString& line)
{
    if (blockIndex < 0 || blockIndex >= currentManifestBlocks_.size())
        return;

    QString modLine = line;
    while (modLine.endsWith('\n') || modLine.endsWith('\r'))
        modLine.chop(1);

    const auto& block = currentManifestBlocks_[blockIndex];
    const int insertAfterLine = block.modifiers.isEmpty()
        ? block.lineNumber : block.modifiers.last().lineNumber;

    QTextBlock textBlock = componentEditor_->document()->findBlockByNumber(insertAfterLine - 1);
    if (!textBlock.isValid())
        return;

    QTextCursor cursor(textBlock);
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText("\n" + modLine);
    componentEditor_->setTextCursor(cursor);
    refreshManifestTable();
    componentDiagnostics_->setPlainText(tr("Added modifier line. Save the file to keep it."));
}

void PluginView::appendManifestBlockSnippet(const QString& snippet)
{
    if (!isPluginManifestFile(currentComponentPath_))
        return;

    QTextCursor cursor = componentEditor_->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(snippet);
    componentEditor_->setTextCursor(cursor);
    refreshManifestTable();
    componentDiagnostics_->setPlainText(
        tr("Added new block at the end of the file. Edit its placeholder names, then save."));
}

bool PluginView::confirmAdvancedFrontendBlock(const QString& kind)
{
    // A Tool/Widget declaration alone does nothing visible - COSMOS loads it
    // as a real web frontend (Vue.js) that has to be written separately, in
    // a codebase this app has no editor for. Without this notice, a
    // beginner adding this block would have every reason to believe they'd
    // just finished making a working Tool/Widget.
    return QMessageBox::question(this, tr("Add %1").arg(kind),
        tr("A %1 needs a real web frontend (Vue.js component) that you write "
           "yourself, outside this app - this only adds its plugin.txt "
           "declaration, not a working %1.\n\nAdd the declaration anyway?")
            .arg(kind),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void PluginView::onNewManifestInterfaceOrRouter(bool isRouter)
{
    if (!isPluginManifestFile(currentComponentPath_))
        return;

    QStringList knownTargets;
    for (const auto& block : currentManifestBlocks_)
        if (block.kind == ViewModels::PluginManifestBlock::Kind::Target && !block.name.isEmpty())
            knownTargets << block.name;

    Dialogs::PluginManifestInterfaceDialog dialog(isRouter, knownTargets, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    appendManifestBlockSnippet(dialog.generatedBlock());
}

void PluginView::onNewManifestMicroservice()
{
    if (!isPluginManifestFile(currentComponentPath_))
        return;

    Dialogs::NewMicroserviceDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    appendManifestBlockSnippet(dialog.generatedBlock());
}

void PluginView::onAddManifestModifierClicked()
{
    const int index = manifestBlockSelectorCombo_->currentIndex();
    if (index < 0 || index >= currentManifestBlocks_.size()) {
        componentDiagnostics_->setPlainText(tr("Select a manifest block first."));
        return;
    }

    const auto& block = currentManifestBlocks_[index];
    QSet<QString> suggested;
    switch (block.kind) {
    case ViewModels::PluginManifestBlock::Kind::Interface:
    case ViewModels::PluginManifestBlock::Kind::Router:
        suggested = ViewModels::Validation::PluginKeywords::interfaceModifiers();
        break;
    case ViewModels::PluginManifestBlock::Kind::Microservice:
        suggested = ViewModels::Validation::PluginKeywords::microserviceModifiers();
        break;
    case ViewModels::PluginManifestBlock::Kind::Target:
        suggested = ViewModels::Validation::PluginKeywords::targetModifiers();
        break;
    case ViewModels::PluginManifestBlock::Kind::Tool:
    case ViewModels::PluginManifestBlock::Kind::Widget:
        suggested = ViewModels::Validation::PluginKeywords::toolModifiers();
        break;
    case ViewModels::PluginManifestBlock::Kind::Variable:
        componentDiagnostics_->setPlainText(tr("VARIABLE does not take modifier lines."));
        return;
    }

    Dialogs::PluginManifestModifierDialog dialog(suggested, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    insertManifestModifierAfterBlock(index, dialog.generatedLine());
}

void PluginView::onDeleteManifestModifierClicked()
{
    const auto selected = manifestTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Select a modifier row first."));
        return;
    }

    const auto* lineCell = manifestTable_->item(selected.first().row(), 0);
    if (!lineCell || !lineCell->data(Qt::UserRole + 1).toBool()) {
        componentDiagnostics_->setPlainText(tr("Select a modifier row (not a block header) first."));
        return;
    }

    const int lineNumber = lineCell->data(Qt::UserRole).toInt();
    const QString modifierText = manifestTable_->item(selected.first().row(), 1)
        ? manifestTable_->item(selected.first().row(), 1)->text().trimmed()
        : tr("modifier");
    const auto answer = QMessageBox::question(
        this,
        tr("Delete Modifier"),
        tr("Delete modifier '%1' from this plugin.txt?").arg(modifierText),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    deleteEditorLine(lineNumber);
    refreshManifestTable();
    componentDiagnostics_->setPlainText(tr("Deleted modifier '%1'. Save the file to keep it.")
        .arg(modifierText));
}

void PluginView::onManifestCellChanged(int row, int column)
{
    if (updatingManifestTable_ || !isPluginManifestFile(currentComponentPath_))
        return;
    if (column != 1 && column != 2)
        return;

    auto* lineCell = manifestTable_->item(row, 0);
    if (!lineCell || !lineCell->data(Qt::UserRole + 1).toBool())
        return; // block header rows aren't editable here (see Apply Block above)

    const int lineNumber = lineCell->data(Qt::UserRole).toInt();
    if (lineNumber <= 0)
        return;

    const QString keyword = manifestTable_->item(row, 1)
        ? manifestTable_->item(row, 1)->text().trimmed().toUpper() : QString();
    const QString args = manifestTable_->item(row, 2)
        ? manifestTable_->item(row, 2)->text().trimmed() : QString();

    if (keyword.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Keyword is required."));
        refreshManifestTable();
        return;
    }

    const QString newLine = args.isEmpty()
        ? QString("  %1").arg(keyword) : QString("  %1 %2").arg(keyword, args);
    replaceEditorLine(lineNumber, newLine);
    componentDiagnostics_->setPlainText(tr("Applied modifier change to line %1. Save the file to keep it.")
        .arg(lineNumber));
    refreshManifestTable();
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
    blockTargetEdit_->setCursorPosition(0);
    blockTargetEdit_->setToolTip(block.targetName);
    blockNameEdit_->setText(block.name);
    blockNameEdit_->setCursorPosition(0);
    blockNameEdit_->setToolTip(block.name);
    blockDescriptionEdit_->setText(block.description);
    // setText() leaves the cursor at the end, so a description longer than the
    // field's width scrolls to show the tail instead of the start - reset the
    // view to the beginning so the field reads naturally on load.
    blockDescriptionEdit_->setCursorPosition(0);
    // A narrow window (Reference panel open, or a small screen) can still
    // clip this field down to a few characters even with the cursor fix
    // above - the tooltip is the one place the full text always stays
    // reachable regardless of available width.
    blockDescriptionEdit_->setToolTip(block.description);

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
        componentDiagnostics_->setPlainText(tr("Select a COMMAND/TELEMETRY block first."));
        return;
    }

    const auto& block = currentBlocks_[index];
    const bool isCommand = block.kind == ViewModels::CmdTlmBlock::Kind::Command;

    const QString target = blockTargetEdit_->text().trimmed().toUpper();
    const QString name   = blockNameEdit_->text().trimmed().toUpper();
    const QString endian = blockEndiannessCombo_->currentText();
    const QString desc   = blockDescriptionEdit_->text().trimmed();

    if (target.isEmpty() || name.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Target and Name are required for a block."));
        return;
    }

    QString line = QString("%1 %2 %3 %4")
        .arg(isCommand ? "COMMAND" : "TELEMETRY", target, name, endian);
    if (!desc.isEmpty())
        line += " " + quotedValue(desc);

    replaceEditorLine(block.lineNumber, line);
    componentDiagnostics_->setPlainText(tr(
        "Updated %1 block at line %2. Comments and fields were preserved. Save to keep it.")
        .arg(isCommand ? "COMMAND" : "TELEMETRY")
        .arg(block.lineNumber));
    refreshStructureTable();
}

void PluginView::onStructureCellChanged(int row, int column)
{
    if (updatingStructureTable_ || !isCmdTlmFile(currentComponentPath_))
        return;
    if (column < 2)
        return;

    applyStructureRowToEditor(row);
}

void PluginView::onToggleReferenceClicked()
{
    if (!guideGroup_)
        return;

    guideGroup_->setVisible(!guideGroup_->isVisible());
    if (toggleReferenceBtn_)
        toggleReferenceBtn_->setChecked(guideGroup_->isVisible());
    QSettings().setValue("PluginView/showReference", guideGroup_->isVisible());
}

void PluginView::onToggleTerminalClicked()
{
    if (!terminalPanel_)
        return;

    terminalPanel_->setVisible(!terminalPanel_->isVisible());
    if (toggleTerminalBtn_)
        toggleTerminalBtn_->setChecked(terminalPanel_->isVisible());
    QSettings().setValue("PluginView/showTerminal", terminalPanel_->isVisible());
}

void PluginView::onDiagnosticItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    const int line = item->data(Qt::UserRole).toInt();
    if (line < 1) return;
    const QTextBlock block = componentEditor_->document()->findBlockByLineNumber(line - 1);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    componentEditor_->setTextCursor(cursor);
    componentEditor_->ensureCursorVisible();
    componentEditor_->setFocus();
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
    const QString originalKeyword = lineCell->data(Qt::UserRole + 3).toString();
    const bool hasExplicitOffset = lineCell->data(Qt::UserRole + 4).toBool();
    const bool isArray = lineCell->data(Qt::UserRole + 5).toBool();
    if (lineNumber <= 0)
        return;

    const auto textAt = [this, row](int col) {
        const auto* item = structureTable_->item(row, col);
        return item ? item->text().trimmed() : QString{};
    };

    const QString keyword = originalKeyword.isEmpty()
        ? (isCommand
        ? (isId ? "APPEND_ID_PARAMETER" : "APPEND_PARAMETER")
        : (isId ? "APPEND_ID_ITEM" : "APPEND_ITEM"))
        : originalKeyword;
    const bool isParameterKeyword = keyword.contains("PARAMETER");
    const QString name = textAt(2).toUpper();
    const QString offset = textAt(3);
    const QString bits = textAt(4);
    const QString type = textAt(5).toUpper();
    const QString arrayBits = textAt(6);
    const QString description = quotedValue(textAt(10));

    if (name.isEmpty() || bits.isEmpty() || type.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Name, Bits, and Type are required."));
        return;
    }
    if (!isValidFieldName(name)) {
        componentDiagnostics_->setPlainText(tr(
            "Field name must start with a letter or underscore and contain only "
            "letters, numbers, and underscores (no spaces or punctuation)."));
        return;
    }
    if (hasExplicitOffset && offset.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Offset is required for PARAMETER / ITEM rows."));
        return;
    }
    if (isArray && arrayBits.isEmpty()) {
        componentDiagnostics_->setPlainText(tr("Array Bits is required for ARRAY rows."));
        return;
    }

    QString replacement;
    if (isCommand) {
        if (isArray && isParameterKeyword && hasExplicitOffset) {
            replacement = QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9 %10")
                .arg(keyword, name, offset, bits, type, arrayBits, textAt(7), textAt(8), textAt(9), description);
        } else if (isArray && isParameterKeyword) {
            replacement = QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9")
                .arg(keyword, name, bits, type, arrayBits, textAt(7), textAt(8), textAt(9), description);
        } else if (isArray && hasExplicitOffset) {
            replacement = QString("  %1 %2 %3 %4 %5 %6 %7")
                .arg(keyword, name, offset, bits, type, arrayBits, description);
        } else if (isArray) {
            replacement = QString("  %1 %2 %3 %4 %5 %6")
                .arg(keyword, name, bits, type, arrayBits, description);
        } else if (hasExplicitOffset) {
            replacement = QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9")
                .arg(keyword, name, offset, bits, type, textAt(7), textAt(8), textAt(9), description);
        } else {
            replacement = QString("  %1 %2 %3 %4 %5 %6 %7 %8")
                .arg(keyword, name, bits, type, textAt(7), textAt(8), textAt(9), description);
        }
    } else {
        // APPEND_ID_ITEM inserts an id_value token right after the type
        // (see CmdTlmParser's own column layout, and CmdTlmFieldDialog's
        // generatedLine()) - the four branches below previously never
        // included it for isId rows at all, so editing any cell of an
        // existing ID telemetry item's row silently dropped its id_value
        // from the file the next time it was saved.
        QStringList toks;
        toks << keyword << name;
        if (hasExplicitOffset) toks << offset;
        toks << bits << type;
        if (isArray) toks << arrayBits;
        if (isId) toks << textAt(9);
        toks << description;
        replacement = "  " + toks.join(' ');
    }

    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid()) {
        componentDiagnostics_->setPlainText(tr("Could not find the selected line in the editor."));
        return;
    }

    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.insertText(replacement);
    componentEditor_->setTextCursor(cursor);
    componentDiagnostics_->setPlainText(tr("Applied field change to line %1. Save the file to keep it.")
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
    componentDiagnostics_->setPlainText(tr("Added field row. Save the file to keep it."));
}

void PluginView::replaceEditorLine(int lineNumber, const QString& newText)
{
    QTextBlock block = componentEditor_->document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid()) {
        componentDiagnostics_->setPlainText(tr("Could not find the selected line in the editor."));
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
    saveComponentBtn_->setText(componentDirty_ ? tr("Save Changes") : tr("Save"));
    saveComponentBtn_->setEnabled(componentDirty_ && !cmdTlmVm_.isBusy());
    updateComponentPathLabel();
}

void PluginView::setCmdTlmActionsVisible(bool visible)
{
    const QList<QWidget*> widgets = {
        startCmdTlmEditBtn_,
        insertMenuBtn_,
        structureMenuBtn_,
        toggleReferenceBtn_
    };

    for (auto* widget : widgets) {
        if (widget)
            widget->setVisible(visible);
    }

    if (componentEditorTabs_) {
        componentEditorTabs_->setTabEnabled(1, visible); // Structure
        if (!visible)
            componentEditorTabs_->setCurrentIndex(0); // Source
    }
    if (guideGroup_) {
        guideGroup_->setVisible(visible && toggleReferenceBtn_ && toggleReferenceBtn_->isChecked());
    }
    updateGroupedActionState();
}

void PluginView::setManifestActionsVisible(bool visible)
{
    if (manifestMenuBtn_)
        manifestMenuBtn_->setVisible(visible);
    if (applyManifestBlockBtn_)
        applyManifestBlockBtn_->setVisible(visible);
    if (manifestBlockSelectorCombo_)
        manifestBlockSelectorCombo_->setVisible(visible);

    if (componentEditorTabs_) {
        componentEditorTabs_->setTabEnabled(2, visible); // Manifest
        if (!visible)
            componentEditorTabs_->setCurrentIndex(0); // Source
    }
}

void PluginView::setScreenPreviewActionsVisible(bool visible)
{
    if (addScreenWidgetBtn_)
        addScreenWidgetBtn_->setVisible(visible);

    if (componentEditorTabs_) {
        componentEditorTabs_->setTabEnabled(3, visible); // Preview
        if (!visible)
            componentEditorTabs_->setCurrentIndex(0); // Source
    }
}

void PluginView::onAddScreenWidgetClicked()
{
    if (!isScreenFile(currentComponentPath_))
        return;

    Dialogs::ScreenWidgetDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QTextCursor cursor = componentEditor_->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n" + dialog.generatedLine());
    componentEditor_->setTextCursor(cursor);
    refreshScreenPreview();
    componentDiagnostics_->setPlainText(
        tr("Added widget at the end of the screen. Save the file to keep it."));
}

void PluginView::refreshScreenPreview()
{
    if (!screenPreview_)
        return;

    if (!isScreenFile(currentComponentPath_)) {
        screenPreview_->setLayoutTree({});
        return;
    }

    const auto result = ViewModels::ScreenLayoutParser::parse(componentEditor_->toPlainText());
    screenPreview_->setLayoutTree(result);
}

void PluginView::updateGroupedActionState()
{
    const bool hasPlugin = tableView_ && tableView_->selectionModel()->hasSelection();
    if (validateBtn_)
        validateBtn_->setVisible(hasPlugin);
    if (moreMenuBtn_)
        moreMenuBtn_->setVisible(hasPlugin);
    if (addTargetAction_)
        addTargetAction_->setEnabled(addTargetBtn_ && addTargetBtn_->isEnabled());
    if (addScriptAction_)
        addScriptAction_->setEnabled(addTargetBtn_ && addTargetBtn_->isEnabled());
    if (removeAction_)
        removeAction_->setEnabled(removeBtn_ && removeBtn_->isEnabled());

    if (validateOfflineAction_)
        validateOfflineAction_->setEnabled(validateOfflineBtn_ && validateOfflineBtn_->isEnabled());
    if (validateMenuBtn_)
        validateMenuBtn_->setEnabled(validateOfflineAction_ && validateOfflineAction_->isEnabled());

    if (insertCmdAction_)
        insertCmdAction_->setEnabled(insertCmdBtn_ && insertCmdBtn_->isEnabled());
    if (insertTlmAction_)
        insertTlmAction_->setEnabled(insertTlmBtn_ && insertTlmBtn_->isEnabled());
    if (addFieldAction_)
        addFieldAction_->setEnabled(addFieldBtn_ && addFieldBtn_->isEnabled());
    if (insertScriptAction_)
        insertScriptAction_->setEnabled(insertScriptBtn_ && insertScriptBtn_->isEnabled());

    const bool anyInsertEnabled = (insertCmdAction_ && insertCmdAction_->isEnabled())
        || (insertTlmAction_ && insertTlmAction_->isEnabled())
        || (addFieldAction_ && addFieldAction_->isEnabled())
        || (insertScriptAction_ && insertScriptAction_->isEnabled());
    if (insertMenuBtn_)
        insertMenuBtn_->setEnabled(anyInsertEnabled);

    if (addStructureFieldAction_)
        addStructureFieldAction_->setEnabled(addStructureFieldBtn_ && addStructureFieldBtn_->isEnabled());
    if (deleteStructureFieldAction_)
        deleteStructureFieldAction_->setEnabled(deleteStructureFieldBtn_ && deleteStructureFieldBtn_->isEnabled());
    if (refreshStructureAction_)
        refreshStructureAction_->setEnabled(refreshStructureBtn_ && refreshStructureBtn_->isEnabled());
    if (applyStructureAction_)
        applyStructureAction_->setEnabled(applyStructureBtn_ && applyStructureBtn_->isEnabled());

    const bool anyStructureEnabled = (addStructureFieldAction_ && addStructureFieldAction_->isEnabled())
        || (deleteStructureFieldAction_ && deleteStructureFieldAction_->isEnabled())
        || (refreshStructureAction_ && refreshStructureAction_->isEnabled())
        || (applyStructureAction_ && applyStructureAction_->isEnabled());
    if (structureMenuBtn_)
        structureMenuBtn_->setEnabled(anyStructureEnabled);
}

void PluginView::updateComponentPathLabel()
{
    if (currentComponentDisplayPath_.isEmpty()) {
        componentPathLabel_->setText(tr("Select a plugin file"));
        return;
    }

    componentPathLabel_->setText(QString("%1  %2")
        .arg(componentDirty_ ? tr("Unsaved") : tr("Editing"), currentComponentDisplayPath_));
}

bool PluginView::confirmDiscardUnsavedChanges()
{
    if (!componentDirty_)
        return true;

    const auto answer = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("This CMD/TLM file has unsaved changes. Open another file and discard them?"),
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
    lines << tr("Validation found %1 error(s). Save anyway?")
                 .arg(result.errorCount());
    int shown = 0;
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.severity != ViewModels::CmdTlmDiagnostic::Severity::Error)
            continue;
        lines << tr("Line %1: %2").arg(diagnostic.line).arg(diagnostic.message);
        if (++shown >= 5)
            break;
    }

    componentDiagnostics_->setPlainText(lines.join('\n'));
    const auto answer = QMessageBox::warning(
        this,
        tr("Validate Before Save"),
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
