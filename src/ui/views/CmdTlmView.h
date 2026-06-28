#pragma once

#include "viewmodels/cmdtlm/CmdTlmViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmParser.h"

#include <QWidget>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>

namespace OpenC3::UI::Widgets { class CmdTlmHighlighter; }

namespace OpenC3::UI::Views {

/// CMD/TLM Editor — browse, edit, and validate COSMOS command/telemetry
/// definition files (.txt) on the remote host.
///
/// Layout:
///   Toolbar  ─ path + browse + insert buttons + validate + save
///   ┌─ File browser ─┬─ Editor (syntax highlighted) ─────────────┐
///   │  (left panel)  │                                            │
///   └────────────────┴────────────────────────────────────────────┘
///   ┌─ Structure tree ─────────────────────────────────────────────┐
///   │  [CMD] COLLECT   [TLM] HEALTH_STATUS   …                    │
///   └──────────────────────────────────────────────────────────────┘
///   ┌─ Diagnostics ────────────────────────────────────────────────┐
///   │  ● Error L45: Unknown data type 'FLOT32'                     │
///   └──────────────────────────────────────────────────────────────┘
class CmdTlmView final : public QWidget {
    Q_OBJECT
public:
    explicit CmdTlmView(ViewModels::CmdTlmViewModel& vm,
                        QWidget*                     parent = nullptr);

public slots:
    void openDirectory(const QString& remotePath);
    void openFile(const QString& remotePath);

private slots:
    void onBrowseClicked();
    void onFileItemDoubleClicked(QListWidgetItem* item);
    void onSaveClicked();
    void onValidateClicked();
    void onInsertCmd();
    void onInsertTlm();
    void onInsertParam();
    void onAddField();
    void onStructureItemClicked(QTreeWidgetItem* item, int col);
    void onDiagnosticItemClicked(QListWidgetItem* item);
    void onFileParsed(const ViewModels::CmdTlmParseResult& result,
                      const QString& filePath);

private:
    void setupUi();
    void bindViewModel();
    void populateStructureTree(const ViewModels::CmdTlmParseResult& result);
    void populateDiagnostics(const ViewModels::CmdTlmParseResult& result);
    void scrollEditorToLine(int line);
    void insertTextAtCursor(const QString& text);

    ViewModels::CmdTlmViewModel& vm_;

    // ── Toolbar ───────────────────────────────────────────────────────────────
    QLabel*      connectionHint_{nullptr};
    QLineEdit*   pathEdit_{nullptr};
    QPushButton* browseBtn_{nullptr};
    QPushButton* insertCmdBtn_{nullptr};
    QPushButton* insertTlmBtn_{nullptr};
    QPushButton* insertParamBtn_{nullptr};
    QPushButton* addFieldBtn_{nullptr};
    QPushButton* validateBtn_{nullptr};
    QPushButton* saveBtn_{nullptr};

    // ── Left panel — file browser ─────────────────────────────────────────────
    QListWidget* fileList_{nullptr};

    // ── Centre — editor ───────────────────────────────────────────────────────
    QLabel*        fileLabel_{nullptr};
    QPlainTextEdit* editor_{nullptr};
    Widgets::CmdTlmHighlighter* highlighter_{nullptr};
    QTextEdit* syntaxGuide_{nullptr};

    // ── Bottom — structure tree ───────────────────────────────────────────────
    QTreeWidget* structureTree_{nullptr};

    // ── Bottom — diagnostics list ─────────────────────────────────────────────
    QListWidget* diagnosticList_{nullptr};
    QLabel*      diagSummary_{nullptr};

    // ── Status bar ────────────────────────────────────────────────────────────
    QLabel* statusLabel_{nullptr};

    QString currentDir_;
    QString currentFile_;
};

} // namespace OpenC3::UI::Views
