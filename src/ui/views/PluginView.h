#pragma once

#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/infra/InfraViewModel.h"
#include "viewmodels/cmdtlm/CmdTlmViewModel.h"

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QListWidget>
#include <QTabWidget>

namespace OpenC3::UI::Widgets { class CmdTlmHighlighter; }

namespace OpenC3::UI::Views {

class PluginView final : public QWidget {
    Q_OBJECT
public:
    explicit PluginView(ViewModels::PluginViewModel& vm,
                        ViewModels::InfraViewModel&  infraVm,
                        ViewModels::CmdTlmViewModel& cmdTlmVm,
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
    void onOpenInCmdTlmClicked();
    void onInsertCmdClicked();
    void onInsertTlmClicked();
    void onAddFieldClicked();

signals:
    void openCmdTlmRequested(const QString& remoteFilePath);

private:
    void setupUi();
    void bindViewModel();
    void populateComponentList(const QStringList& files, const QString& pluginRootPath);
    void openSelectedComponent(QListWidgetItem* item);
    void insertTextAtCursor(const QString& text);

    [[nodiscard]] QString selectedPluginName() const;
    [[nodiscard]] QString selectedPluginRoot() const;
    [[nodiscard]] QString selectedComponentPath() const;

    ViewModels::PluginViewModel& vm_;
    ViewModels::InfraViewModel&  infraVm_;
    ViewModels::CmdTlmViewModel& cmdTlmVm_;

    QTableView*  tableView_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QPushButton* installBtn_{nullptr};
    QPushButton* removeBtn_{nullptr};
    QPushButton* validateBtn_{nullptr};
    QPushButton* buildBtn_{nullptr};
    QPushButton* scaffoldBtn_{nullptr};
    QPushButton* addTargetBtn_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QTabWidget*  detailTabs_{nullptr};
    QTextEdit*   detailEdit_{nullptr};
    QListWidget* componentList_{nullptr};
    QLabel*      componentPathLabel_{nullptr};
    QTextEdit*   componentEditor_{nullptr};
    QTextEdit*   componentDiagnostics_{nullptr};
    QTextEdit*   componentGuide_{nullptr};
    QPushButton* openComponentBtn_{nullptr};
    QPushButton* saveComponentBtn_{nullptr};
    QPushButton* validateComponentBtn_{nullptr};
    QPushButton* openInCmdTlmBtn_{nullptr};
    QPushButton* insertCmdBtn_{nullptr};
    QPushButton* insertTlmBtn_{nullptr};
    QPushButton* addFieldBtn_{nullptr};
    Widgets::CmdTlmHighlighter* highlighter_{nullptr};
    QString     currentPluginRoot_;
    QString     currentComponentPath_;
};

} // namespace OpenC3::UI::Views
