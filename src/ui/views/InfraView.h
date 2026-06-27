#pragma once

#include "viewmodels/infra/InfraViewModel.h"

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QComboBox>

namespace OpenC3::UI::Views {

/// Infrastructure Manager view.
/// Three tabs: ENV editor | compose.yaml editor | Patch generator.
class InfraView final : public QWidget {
    Q_OBJECT
public:
    explicit InfraView(ViewModels::InfraViewModel& vm, QWidget* parent = nullptr);

private slots:
    void onEnvLoad();
    void onEnvSave();
    void onComposeLoad();
    void onComposeSave();
    void onGeneratePatch();
    void onSavePatch();
    void onTableCellChanged(int row, int col);

private:
    void setupUi();
    void bindViewModel();

    QWidget* buildEnvTab();
    QWidget* buildComposeTab();
    QWidget* buildPatchTab();

    void loadEnvIntoTable(const QString& content);
    QString collectTableToEnv() const;
    void syncTableToRaw();
    void syncRawToTable();
    void appendPatchLine(const QString& line);

    ViewModels::InfraViewModel& vm_;

    QTabWidget* tabs_{nullptr};

    // ── ENV tab ───────────────────────────────────────────────────────────────
    QLineEdit*    envPathEdit_{nullptr};
    QPushButton*  envLoadBtn_{nullptr};
    QPushButton*  envSaveBtn_{nullptr};
    QTableWidget* envTable_{nullptr};
    QPlainTextEdit* envRawEdit_{nullptr};
    QPushButton*  envSyncToRawBtn_{nullptr};
    QPushButton*  envSyncToTableBtn_{nullptr};
    bool          suppressTableSignal_{false};

    // ── Compose tab ───────────────────────────────────────────────────────────
    QLineEdit*      composePath_{nullptr};
    QPushButton*    composeLoadBtn_{nullptr};
    QPushButton*    composeSaveBtn_{nullptr};
    QPlainTextEdit* composeEdit_{nullptr};

    // ── Patch tab ─────────────────────────────────────────────────────────────
    QComboBox*    patchSourceCombo_{nullptr};
    QTextEdit*    patchEdit_{nullptr};
    QPushButton*  patchGenBtn_{nullptr};
    QPushButton*  patchSaveBtn_{nullptr};
    QLabel*       statusLabel_{nullptr};

    // Baselines captured at load time for diff
    QString envBaseline_;
    QString composeBaseline_;
    QString envCurrentPath_;
    QString composeCurrentPath_;
};

} // namespace OpenC3::UI::Views
