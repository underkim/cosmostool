#pragma once

#include "viewmodels/infra/InfraViewModel.h"

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>

namespace OpenC3::UI::Dialogs {

/// Dialog for adding a new target to an existing plugin.
/// Creates targets/<TARGET>/ directory tree (cmd_tlm, screens, procedures)
/// inside an already-existing plugin root directory.
class AddTargetDialog final : public QDialog {
    Q_OBJECT
public:
    explicit AddTargetDialog(
        ViewModels::InfraViewModel& vm,
        const QString&              pluginRoot,  // remote path to plugin root dir
        QWidget*                    parent = nullptr);

private slots:
    void onCreate();
    void onTargetNameChanged();

private:
    ViewModels::InfraViewModel& vm_;
    QString pluginRoot_;

    QLineEdit*    targetNameEdit_{nullptr};
    QLineEdit*    namespaceEdit_{nullptr};
    QRadioButton* tmplGenericBtn_{nullptr};
    QRadioButton* tmplCcsdsBtn_{nullptr};
    QLabel*       previewLabel_{nullptr};
    QLabel*       statusLabel_{nullptr};
    QPushButton*  createBtn_{nullptr};
    QButtonGroup* tmplGroup_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
