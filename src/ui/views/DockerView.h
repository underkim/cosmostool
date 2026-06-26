#pragma once

#include "viewmodels/docker/DockerViewModel.h"
#include "ui/widgets/LogWidget.h"

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>

namespace OpenC3::UI::Views {

class DockerView final : public QWidget {
    Q_OBJECT
public:
    explicit DockerView(
        ViewModels::DockerViewModel& vm,
        QWidget* parent = nullptr);

private slots:
    void onTableSelectionChanged();
    void onStartClicked();
    void onStopClicked();
    void onRestartClicked();
    void onRemoveClicked();
    void onViewLogsClicked();

private:
    void setupUi();
    void bindViewModel();
    [[nodiscard]] QString selectedContainerName() const;

    ViewModels::DockerViewModel& vm_;

    QTableView*          tableView_{nullptr};
    QPushButton*         startBtn_{nullptr};
    QPushButton*         stopBtn_{nullptr};
    QPushButton*         restartBtn_{nullptr};
    QPushButton*         removeBtn_{nullptr};
    QPushButton*         logsBtn_{nullptr};
    QPushButton*         refreshBtn_{nullptr};
    Widgets::LogWidget*  logWidget_{nullptr};
    QLabel*              versionLabel_{nullptr};
};

} // namespace OpenC3::UI::Views
