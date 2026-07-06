#pragma once

#include "viewmodels/infra/InfraViewModel.h"

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

namespace OpenC3::UI::Dialogs {

/// Dialog for adding a new procedures/<name>.rb script to an existing
/// target - the single-file counterpart to AddTargetDialog. Beginner-facing:
/// picks an existing target from a combo (rather than typing a path) and
/// generates a boilerplate cmd()/wait_check() procedure, matching the
/// checkout script PluginTemplateEngine::buildTargetFiles() already
/// generates for a brand-new target.
class NewScriptDialog final : public QDialog {
    Q_OBJECT
public:
    explicit NewScriptDialog(
        ViewModels::InfraViewModel& vm,
        const QString&              pluginRoot,   // remote path to plugin root dir
        const QStringList&          knownTargets,
        QWidget*                    parent = nullptr);

private slots:
    void onCreate();
    void updatePreview();

private:
    ViewModels::InfraViewModel& vm_;
    QString pluginRoot_;

    QComboBox*   targetCombo_{nullptr};
    QLineEdit*   scriptNameEdit_{nullptr};
    QLabel*      previewLabel_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QPushButton* createBtn_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
