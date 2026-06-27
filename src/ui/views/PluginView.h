#pragma once

#include "viewmodels/plugin/PluginViewModel.h"
#include "viewmodels/infra/InfraViewModel.h"

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>

namespace OpenC3::UI::Views {

class PluginView final : public QWidget {
    Q_OBJECT
public:
    explicit PluginView(ViewModels::PluginViewModel& vm,
                        ViewModels::InfraViewModel&  infraVm,
                        QWidget*                     parent = nullptr);

private slots:
    void onInstallClicked();
    void onRemoveClicked();
    void onValidateClicked();
    void onScaffoldClicked();
    void onTableSelectionChanged();

private:
    void setupUi();
    void bindViewModel();

    [[nodiscard]] QString selectedPluginName() const;

    ViewModels::PluginViewModel& vm_;
    ViewModels::InfraViewModel&  infraVm_;

    QTableView*  tableView_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QPushButton* installBtn_{nullptr};
    QPushButton* removeBtn_{nullptr};
    QPushButton* validateBtn_{nullptr};
    QPushButton* scaffoldBtn_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel*      statusLabel_{nullptr};
    QTextEdit*   detailEdit_{nullptr};
};

} // namespace OpenC3::UI::Views
