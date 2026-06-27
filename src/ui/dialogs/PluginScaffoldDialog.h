#pragma once

#include "viewmodels/infra/InfraViewModel.h"

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>

namespace OpenC3::UI::Dialogs {

/// Wizard dialog for scaffolding a new OpenC3 COSMOS plugin on the remote host.
///
/// Steps:
///   1. Enter plugin name, target name, namespace, description
///   2. Select template type (Generic / Satellite / GSE Interface / Tool)
///   3. Preview the file tree to be created
///   4. Confirm → calls InfraViewModel::scaffoldPlugin()
class PluginScaffoldDialog final : public QDialog {
    Q_OBJECT
public:
    explicit PluginScaffoldDialog(
        ViewModels::InfraViewModel& vm,
        QWidget*                    parent = nullptr);

private slots:
    void onCreate();
    void updatePreview();

private:
    void setupUi();
    void bindViewModel();
    QString buildPreviewTree() const;

    ViewModels::InfraViewModel& vm_;

    QLineEdit*    pluginNameEdit_{nullptr};
    QLineEdit*    targetNameEdit_{nullptr};
    QLineEdit*    namespaceEdit_{nullptr};
    QLineEdit*    descriptionEdit_{nullptr};
    QLineEdit*    remoteRootEdit_{nullptr};

    QButtonGroup* templateGroup_{nullptr};
    QRadioButton* tmplGenericBtn_{nullptr};
    QRadioButton* tmplSatelliteBtn_{nullptr};
    QRadioButton* tmplInterfaceBtn_{nullptr};
    QRadioButton* tmplToolBtn_{nullptr};

    QTextEdit*    previewEdit_{nullptr};
    QLabel*       statusLabel_{nullptr};

    QPushButton*  previewBtn_{nullptr};
    QPushButton*  createBtn_{nullptr};
    QPushButton*  cancelBtn_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
