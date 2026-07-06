#pragma once

#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/plugin/PluginManifestParser.h"
#include "viewmodels/infra/InfraViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmViewModel.h"
#include "viewmodels/validator/ValidatorViewModel.h"
#include "viewmodels/logviewer/LogViewerViewModel.h"
#include "ui/widgets/ScreenPreviewWidget.h"

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QListWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QVector>
#include <QAction>
#include <QStackedWidget>

namespace OpenC3::UI::Widgets { class CmdTlmHighlighter; }

namespace OpenC3::UI::Views {

class LogViewerView;

class PluginView final : public QWidget {
    Q_OBJECT
public:
    // Which range of the wizard's 5 steps the step strip currently shows.
    // Creation (steps 1-3: Plugin/File/Edit) is reachable from the app's
    // Plugin Creation mode without a connection; CheckBuild (steps 4-5:
    // Check/Build & Install) is reachable from Connect & Operate mode and
    // needs a real connection (openc3cli, gem build). Both modes act on the
    // exact same underlying session state (currentPluginRoot_,
    // currentComponentPath_, componentEditor_ contents, etc.) - switching
    // modes never resets or loses an open file's edits.
    enum class StepStripMode { Creation, CheckBuild };

    explicit PluginView(ViewModels::PluginViewModel& vm,
                        ViewModels::InfraViewModel&  infraVm,
                        ViewModels::CmdTlmViewModel& cmdTlmVm,
                        ViewModels::ValidatorViewModel& validatorVm,
                        ViewModels::LogViewerViewModel& logViewerVm,
                        QWidget*                     parent = nullptr);

    void setStepStripMode(StepStripMode mode);

private slots:
    void onInstallClicked();
    void onRemoveClicked();
    void onValidateClicked();
    void onBuildClicked();
    void onScaffoldClicked();
    void onAddTargetClicked();
    void onAddScriptClicked();
    void onInsertScriptClicked();
    void onTableSelectionChanged();
    void restoreSelectionAfterRefresh();
    void onComponentItemDoubleClicked(QListWidgetItem* item);
    void onOpenComponentClicked();
    void onSaveComponentClicked();
    void onValidateComponentClicked();
    void onValidateOfflineClicked();
    void onStartCmdTlmEditClicked();
    void onInsertCmdClicked();
    void onInsertTlmClicked();
    void onAddFieldClicked();
    void onAddStructureFieldClicked();
    void onDeleteStructureFieldClicked();
    void onRefreshStructureClicked();
    void onApplyStructureClicked();
    void onBlockSelectionChanged(int index);
    void onApplyBlockClicked();
    void onStructureCellChanged(int row, int column);
    void onManifestBlockSelectionChanged(int index);
    void onApplyManifestBlockClicked();
    void onAddManifestModifierClicked();
    void onDeleteManifestModifierClicked();
    void onManifestCellChanged(int row, int column);
    void onToggleReferenceClicked();
    void onBrowseGoClicked();
    void onBrowseItemDoubleClicked(QListWidgetItem* item);
    void onToggleTerminalClicked();
    void onDiagnosticItemClicked(QListWidgetItem* item);

signals:
    /// Hand the current component buffer to the Validator view for full per-rule
    /// (offline) analysis — distinct from the runtime gem build validation.
    void openInValidatorRequested(const QString& content);

private:
    void setupUi();
    void bindViewModel();
    void goToWizardStep(int step);
    void updateWizardStepStrip();
    void populateComponentList(const QStringList& files, const QString& pluginRootPath);
    void setComponentHint(const QString& text);
    void openSelectedComponent(QListWidgetItem* item);
    void openBrowsePath(const QString& remotePath);
    void insertTextAtCursor(const QString& text);
    void refreshStructureTable();
    void refreshBlockEditor();
    void populateBlockForm(int blockIndex);
    void refreshManifestTable();
    void refreshManifestBlockEditor();
    void populateManifestBlockForm(int blockIndex);
    void insertManifestModifierAfterBlock(int blockIndex, const QString& line);
    void appendManifestBlockSnippet(const QString& snippet);
    [[nodiscard]] bool confirmAdvancedFrontendBlock(const QString& kind);
    void onNewManifestInterfaceOrRouter(bool isRouter);
    void onNewManifestMicroservice();
    void onAddScreenWidgetClicked();
    void setManifestActionsVisible(bool visible);
    void refreshScreenPreview();
    void setScreenPreviewActionsVisible(bool visible);
    void applyStructureRowToEditor(int row);
    void focusEditorLineForStructureRow(int row);
    void insertStructureFieldAfterRow(int row, const QString& line);
    void replaceEditorLine(int lineNumber, const QString& newText);
    void deleteEditorLine(int lineNumber);
    void setComponentDirty(bool dirty);
    void setCmdTlmActionsVisible(bool visible);
    void updateActionHints();
    void updateComponentEmptyState();
    void updateComponentPathLabel();
    void updateGroupedActionState();
    [[nodiscard]] bool confirmDiscardUnsavedChanges();
    [[nodiscard]] bool confirmSaveAfterValidation();

    [[nodiscard]] QString selectedPluginName() const;
    [[nodiscard]] QString selectedPluginRoot() const;
    [[nodiscard]] QString selectedComponentPath() const;

    ViewModels::PluginViewModel& vm_;
    ViewModels::InfraViewModel&  infraVm_;
    ViewModels::CmdTlmViewModel& cmdTlmVm_;
    ViewModels::ValidatorViewModel& validatorVm_;
    ViewModels::LogViewerViewModel& logViewerVm_;

    QTableView*  tableView_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QPushButton* installBtn_{nullptr};
    QPushButton* removeBtn_{nullptr};
    QPushButton* validateBtn_{nullptr};
    QPushButton* buildBtn_{nullptr};
    QPushButton* scaffoldBtn_{nullptr};
    QPushButton* addTargetBtn_{nullptr};
    QToolButton* moreMenuBtn_{nullptr};
    QAction*     addTargetAction_{nullptr};
    QAction*     addScriptAction_{nullptr};
    QAction*     removeAction_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QLabel*      pluginSummaryLabel_{nullptr};
    QTabWidget*  detailTabs_{nullptr};
    QTabWidget*  componentEditorTabs_{nullptr};
    QTextEdit*   detailEdit_{nullptr};
    QTabWidget*  filesTabs_{nullptr};
    QListWidget* componentList_{nullptr};
    QLabel*      componentHintLabel_{nullptr};
    QLabel*      componentListEmptyLabel_{nullptr};
    // "Browse" tab: unrestricted remote directory browsing (not scoped to the
    // selected plugin's root), reusing cmdTlmVm_.listDirectory()/openFile()
    // the same way the (now-retired) standalone CMD/TLM screen did.
    QLineEdit*   browsePathEdit_{nullptr};
    QPushButton* browseGoBtn_{nullptr};
    QListWidget* browseList_{nullptr};
    QLabel*      browseEmptyLabel_{nullptr};
    QString      currentBrowseDir_;
    QLabel*      componentPathLabel_{nullptr};
    QTextEdit*   componentEditor_{nullptr};
    QTextEdit*   componentDiagnostics_{nullptr};
    // Itemized, click-to-jump, severity-colored diagnostics from the last
    // CMD/TLM parse or offline validation - richer than componentDiagnostics_
    // (which stays a free-text status line for one-off messages like
    // "Loaded"/"Saved"), ported from CmdTlmView's diagnosticList_.
    QListWidget* diagnosticList_{nullptr};
    QLabel*      diagnosticListEmptyLabel_{nullptr};
    QTableWidget* structureTable_{nullptr};
    QComboBox*   blockSelectorCombo_{nullptr};
    QLabel*      blockKindLabel_{nullptr};
    QLineEdit*   blockTargetEdit_{nullptr};
    QLineEdit*   blockNameEdit_{nullptr};
    QComboBox*   blockEndiannessCombo_{nullptr};
    QLineEdit*   blockDescriptionEdit_{nullptr};
    QPushButton* applyBlockBtn_{nullptr};
    QGroupBox*   guideGroup_{nullptr};
    QPushButton* toggleReferenceBtn_{nullptr};
    QTextEdit*   componentGuide_{nullptr};
    QPushButton* openComponentBtn_{nullptr};
    QPushButton* saveComponentBtn_{nullptr};
    QPushButton* validateComponentBtn_{nullptr};
    QPushButton* validateOfflineBtn_{nullptr};
    QToolButton* validateMenuBtn_{nullptr};
    QAction*     validateOfflineAction_{nullptr};
    QPushButton* startCmdTlmEditBtn_{nullptr};
    QPushButton* insertCmdBtn_{nullptr};
    QPushButton* insertTlmBtn_{nullptr};
    QPushButton* addFieldBtn_{nullptr};
    QToolButton* insertMenuBtn_{nullptr};
    QAction*     insertCmdAction_{nullptr};
    QAction*     insertTlmAction_{nullptr};
    QAction*     addFieldAction_{nullptr};
    QPushButton* insertScriptBtn_{nullptr};
    QAction*     insertScriptAction_{nullptr};
    QPushButton* addStructureFieldBtn_{nullptr};
    QPushButton* deleteStructureFieldBtn_{nullptr};
    QPushButton* refreshStructureBtn_{nullptr};
    QPushButton* applyStructureBtn_{nullptr};
    QToolButton* structureMenuBtn_{nullptr};
    QAction*     addStructureFieldAction_{nullptr};
    QAction*     deleteStructureFieldAction_{nullptr};
    QAction*     refreshStructureAction_{nullptr};
    QAction*     applyStructureAction_{nullptr};
    Widgets::CmdTlmHighlighter* highlighter_{nullptr};
    // Bottom "Terminal" panel: the existing Logs screen's widget/ViewModel,
    // embedded here for convenience - same one-way log-streaming mechanism,
    // no new interactive/PTY terminal.
    QPushButton* toggleTerminalBtn_{nullptr};
    QWidget*     terminalPanel_{nullptr};
    LogViewerView* terminalView_{nullptr};
    QString     currentPluginRoot_;
    // Set right before cmdTlmVm_.openFile() is called (from either the
    // plugin-scoped list or the Browse tab); the fileOpened handler only
    // reacts if the reported path matches, instead of prefix-matching
    // currentPluginRoot_ - lets a single view safely handle opens that
    // originate outside the current plugin's root.
    QString     pendingOpenPath_;
    QString     currentComponentPath_;
    QString     currentComponentDisplayPath_;
    QString     firstCmdTlmComponentPath_;
    bool        componentDirty_{false};
    bool        pendingBuild_{false};
    bool        pendingOfflineValidation_{false};
    bool        updatingStructureTable_{false};
    QVector<ViewModels::CmdTlmBlock> currentBlocks_;

    // ── Manifest tab (plugin.txt: TARGET/INTERFACE/ROUTER/MICROSERVICE/TOOL/
    // WIDGET/VARIABLE) ──────────────────────────────────────────────────────
    QTableWidget* manifestTable_{nullptr};
    QVector<ViewModels::PluginManifestBlock> currentManifestBlocks_;
    bool         updatingManifestTable_{false};
    QComboBox*   manifestBlockSelectorCombo_{nullptr};
    QLabel*      manifestKindLabel_{nullptr};
    QLineEdit*   manifestNameEdit_{nullptr};
    QLineEdit*   manifestClassOrFolderEdit_{nullptr};
    QLineEdit*   manifestArgsEdit_{nullptr};
    QPushButton* applyManifestBlockBtn_{nullptr};
    QToolButton* manifestMenuBtn_{nullptr};
    QAction*     addManifestModifierAction_{nullptr};
    QAction*     deleteManifestModifierAction_{nullptr};
    QAction*     refreshManifestAction_{nullptr};

    // ── Preview tab (screens/*.txt static layout preview) ──────────────────
    Widgets::ScreenPreviewWidget* screenPreview_{nullptr};
    QPushButton* addScreenWidgetBtn_{nullptr};

    // Set right before any vm_.refresh() call (Refresh button, New Plugin,
    // Add Target, or the scaffoldComplete/targetAdded InfraViewModel
    // callbacks). PluginTableModel::setPlugins() does beginResetModel()/
    // endResetModel(), which silently clears the table's selection model
    // *without* emitting selectionChanged (confirmed empirically - Qt's
    // reset-driven clear bypasses the normal select()/signal path) - so
    // restoreSelectionAfterRefresh(), connected to the always-reliable
    // pluginListChanged() signal, is what re-selects the right row
    // afterwards, not onTableSelectionChanged() reacting to the reset itself.
    bool        refreshingPluginList_{false};

    // Snapshot of currentPluginRoot_ taken at the *start* of a refresh
    // cycle, before any handler (e.g. scaffoldComplete) has a chance to
    // reassign currentPluginRoot_ to a different (e.g. newly-created)
    // plugin. onTableSelectionChanged() compares the plugin reselected
    // after refresh against *this*, not against currentPluginRoot_, to
    // tell "the same plugin got reselected after a routine refresh"
    // (preserve the open file/editing session) apart from "refresh also
    // switched to a genuinely different plugin" (reset is correct there).
    QString     preRefreshPluginRoot_;

    // Step-by-step wizard scaffold (Phase 0): Plugin -> File -> Edit -> Check
    // -> Build & Install. Phase 0 only wires navigation with placeholder
    // pages; later phases move the real widgets above into wizardStack_'s
    // pages one at a time.
    static constexpr int kWizardStepPlugin = 0;
    static constexpr int kWizardStepFile   = 1;
    static constexpr int kWizardStepEdit   = 2;
    static constexpr int kWizardStepCheck  = 3;
    static constexpr int kWizardStepBuild  = 4;

    QVector<QPushButton*> wizardStepButtons_;
    QStackedWidget*        wizardStack_{nullptr};
    QPushButton*           wizardBackBtn_{nullptr};
    QPushButton*           wizardNextBtn_{nullptr};
    int                    currentWizardStep_{kWizardStepPlugin};
    QLabel*                breadcrumbLabel_{nullptr};
    QLabel*                pageTitleLabel_{nullptr};
    QLabel*                pageSubtitleLabel_{nullptr};
    StepStripMode          stepStripMode_{StepStripMode::Creation};

    // Phase 6: forward gating - returns the furthest step reachable given
    // current selections (no plugin selected -> File is out of reach; no
    // file open -> Edit/Check/Build are out of reach). Back is always free.
    int maxReachableWizardStep() const;
    void updateWizardBreadcrumb();
    [[nodiscard]] bool stepVisibleInCurrentMode(int step) const noexcept;
};

} // namespace OpenC3::UI::Views
