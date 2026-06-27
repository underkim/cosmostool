#pragma once

#include "viewmodels/infra/InfraViewModel.h"

#include <QDialog>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QMap>

namespace OpenC3::UI::Dialogs {

/// Multi-step plugin creation wizard.
///
/// Step 0 — Plugin Info  (name, description, remote root)
/// Step 1 — Target Setup (target name, interface type, CCSDS toggle)
/// Step 2 — File Preview (editable tabs, one per generated file)
///
/// On Finish → calls InfraViewModel::scaffoldPlugin() using the (possibly
/// user-edited) file contents.
class PluginWizard final : public QDialog {
    Q_OBJECT
public:
    explicit PluginWizard(ViewModels::InfraViewModel& vm,
                          QWidget*                    parent = nullptr);

private slots:
    void onNext();
    void onBack();
    void onFinish();
    void onPluginNameChanged();
    void onTargetNameChanged();
    void refreshPreview();

private:
    void buildInfoPage();
    void buildTargetPage();
    void buildPreviewPage();
    void goToStep(int step);
    void updateNavButtons();

    // Derived names
    [[nodiscard]] QString gemName()   const;
    [[nodiscard]] QString targetUpper() const;
    [[nodiscard]] int     templateType() const;

    ViewModels::InfraViewModel& vm_;

    QStackedWidget* pages_{nullptr};
    QPushButton*    backBtn_{nullptr};
    QPushButton*    nextBtn_{nullptr};
    QPushButton*    finishBtn_{nullptr};
    QLabel*         stepIndicator_{nullptr};
    QLabel*         statusLabel_{nullptr};

    int currentStep_{0};

    // ── Page 0 — Plugin Info ──────────────────────────────────────────────────
    QLineEdit* pluginNameEdit_{nullptr};
    QLineEdit* descriptionEdit_{nullptr};
    QLineEdit* remoteRootEdit_{nullptr};
    QLabel*    gemNameLabel_{nullptr};

    // ── Page 1 — Target Setup ─────────────────────────────────────────────────
    QLineEdit*    targetNameEdit_{nullptr};
    QLineEdit*    namespaceEdit_{nullptr};
    QComboBox*    ifaceTypeCombo_{nullptr};
    QLineEdit*    ifaceHostEdit_{nullptr};
    QLineEdit*    ifacePortEdit_{nullptr};
    QRadioButton* tmplGenericBtn_{nullptr};
    QRadioButton* tmplCcsdsBtn_{nullptr};
    QRadioButton* tmplGseBtn_{nullptr};
    QButtonGroup* tmplGroup_{nullptr};
    QLabel*       targetPreviewLabel_{nullptr};

    // ── Page 2 — File Preview ─────────────────────────────────────────────────
    QTabWidget*                    previewTabs_{nullptr};
    QMap<QString, QPlainTextEdit*> fileEditors_;   // relativePath → editor
};

} // namespace OpenC3::UI::Dialogs
