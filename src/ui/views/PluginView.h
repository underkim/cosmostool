#pragma once

#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/infra/InfraViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmViewModel.h"
#include "viewmodels/validator/ValidatorViewModel.h"

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

namespace OpenC3::UI::Widgets { class CmdTlmHighlighter; }

namespace OpenC3::UI::Views {

class PluginView final : public QWidget {
    Q_OBJECT
public:
    explicit PluginView(ViewModels::PluginViewModel& vm,
                        ViewModels::InfraViewModel&  infraVm,
                        ViewModels::CmdTlmViewModel& cmdTlmVm,
                        ViewModels::ValidatorViewModel& validatorVm,
                        QWidget*                     parent = nullptr);

private slots:
    void onInstallClicked();
    void onRemoveClicked();
    void onValidateClicked();
    void onBuildClicked();
    void onScaffoldClicked();
    void onAddTargetClicked();
    void onTableSelectionChanged();
    void onComponentItemDoubleClicked(QListWidgetItem* item);
    void onOpenComponentClicked();
    void onSaveComponentClicked();
    void onValidateComponentClicked();
    void onValidateOfflineClicked();
    void onOpenInCmdTlmClicked();
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
    void onToggleReferenceClicked();

signals:
    void openCmdTlmRequested(const QString& remoteFilePath);
    /// Hand the current component buffer to the Validator view for full per-rule
    /// (offline) analysis — distinct from the runtime gem build validation.
    void openInValidatorRequested(const QString& content);

private:
    void setupUi();
    void bindViewModel();
    void populateComponentList(const QStringList& files, const QString& pluginRootPath);
    void setComponentHint(const QString& text);
    void openSelectedComponent(QListWidgetItem* item);
    void insertTextAtCursor(const QString& text);
    void refreshStructureTable();
    void refreshBlockEditor();
    void populateBlockForm(int blockIndex);
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
    void updateWorkflowHint();
    [[nodiscard]] bool confirmDiscardUnsavedChanges();
    [[nodiscard]] bool confirmSaveAfterValidation();

    [[nodiscard]] QString selectedPluginName() const;
    [[nodiscard]] QString selectedPluginRoot() const;
    [[nodiscard]] QString selectedComponentPath() const;

    ViewModels::PluginViewModel& vm_;
    ViewModels::InfraViewModel&  infraVm_;
    ViewModels::CmdTlmViewModel& cmdTlmVm_;
    ViewModels::ValidatorViewModel& validatorVm_;

    QTableView*  tableView_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QPushButton* installBtn_{nullptr};
    QPushButton* removeBtn_{nullptr};
    QPushButton* validateBtn_{nullptr};
    QPushButton* buildBtn_{nullptr};
    QPushButton* scaffoldBtn_{nullptr};
    QPushButton* addTargetBtn_{nullptr};
    QWidget*     selectedPluginActions_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QLabel*      pluginSummaryLabel_{nullptr};
    QLabel*      workflowHintLabel_{nullptr};
    QTabWidget*  detailTabs_{nullptr};
    QTabWidget*  componentEditorTabs_{nullptr};
    QTextEdit*   detailEdit_{nullptr};
    QListWidget* componentList_{nullptr};
    QLabel*      componentHintLabel_{nullptr};
    QLabel*      componentListEmptyLabel_{nullptr};
    QLabel*      componentPathLabel_{nullptr};
    QTextEdit*   componentEditor_{nullptr};
    QTextEdit*   componentDiagnostics_{nullptr};
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
    QPushButton* openInCmdTlmBtn_{nullptr};
    QPushButton* startCmdTlmEditBtn_{nullptr};
    QPushButton* insertCmdBtn_{nullptr};
    QPushButton* insertTlmBtn_{nullptr};
    QPushButton* addFieldBtn_{nullptr};
    QToolButton* insertMenuBtn_{nullptr};
    QAction*     insertCmdAction_{nullptr};
    QAction*     insertTlmAction_{nullptr};
    QAction*     addFieldAction_{nullptr};
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
    QString     currentPluginRoot_;
    QString     currentComponentPath_;
    QString     currentComponentDisplayPath_;
    QString     firstCmdTlmComponentPath_;
    bool        componentDirty_{false};
    bool        pendingBuild_{false};
    bool        pendingOfflineValidation_{false};
    bool        updatingStructureTable_{false};
    QVector<ViewModels::CmdTlmBlock> currentBlocks_;
};

} // namespace OpenC3::UI::Views
