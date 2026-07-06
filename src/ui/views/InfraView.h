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
#include <QCheckBox>

namespace OpenC3::UI::Views {

/// Infrastructure Manager view.
/// Three tabs: ENV editor | compose.yaml editor | Volume Override (patch).
class InfraView final : public QWidget {
    Q_OBJECT
public:
    explicit InfraView(ViewModels::InfraViewModel& vm, QWidget* parent = nullptr);

private slots:
    void onEnvLoad();
    void onEnvSave();
    void onComposeLoad();
    void onComposeSave();
    void onTableCellChanged(int row, int col);
    void onExtractFile();
    void onApplyOverride();
    void onContainerFilePathChanged(const QString& path);

private:
    void setupUi();
    void bindViewModel();

    QWidget* buildEnvTab();
    QWidget* buildComposeTab();
    QWidget* buildVolumeOverrideTab();

    void loadEnvIntoTable(const QString& content);
    QString collectTableToEnv() const;
    void syncTableToRaw();
    void syncRawToTable();
    void applySecretMasking();

    ViewModels::InfraViewModel& vm_;

    QTabWidget* tabs_{nullptr};
    QLabel*     statusLabel_{nullptr};

    // ── ENV tab ───────────────────────────────────────────────────────────────
    QLineEdit*      envPathEdit_{nullptr};
    QPushButton*    envLoadBtn_{nullptr};
    QPushButton*    envSaveBtn_{nullptr};
    QTableWidget*   envTable_{nullptr};
    QPlainTextEdit* envRawEdit_{nullptr};
    QPushButton*    envSyncToRawBtn_{nullptr};
    QPushButton*    envSyncToTableBtn_{nullptr};
    QCheckBox*      showSecretsCheck_{nullptr};
    bool            suppressTableSignal_{false};

    // ── Compose tab ───────────────────────────────────────────────────────────
    QLineEdit*      composePath_{nullptr};
    QPushButton*    composeLoadBtn_{nullptr};
    QPushButton*    composeSaveBtn_{nullptr};
    QPlainTextEdit* composeEdit_{nullptr};

    // ── Volume Override tab ───────────────────────────────────────────────────
    QComboBox*      containerCombo_{nullptr};
    QPushButton*    containerRefreshBtn_{nullptr};
    QLineEdit*      containerFilePathEdit_{nullptr};
    QPushButton*    extractBtn_{nullptr};
    QPlainTextEdit* containerFileEdit_{nullptr};
    QLineEdit*      hostSavePathEdit_{nullptr};
    QPushButton*    applyOverrideBtn_{nullptr};
    QPlainTextEdit* volumeEntryEdit_{nullptr};
    QPushButton*    copyVolumeEntryBtn_{nullptr};
    QPushButton*    insertToComposeBtn_{nullptr};
};

} // namespace OpenC3::UI::Views
