#pragma once

#include "viewmodels/doctor/DoctorViewModel.h"
#include "ui/widgets/StatusBadge.h"

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>

namespace OpenC3::UI::Views {

class DoctorView final : public QWidget {
    Q_OBJECT
public:
    explicit DoctorView(
        ViewModels::DoctorViewModel& vm,
        QWidget* parent = nullptr);

private:
    void setupUi();
    void bindViewModel();

    ViewModels::DoctorViewModel& vm_;

    QPushButton*         runBtn_{nullptr};
    QProgressBar*        progressBar_{nullptr};
    QLabel*              summaryLabel_{nullptr};
    QTableView*          tableView_{nullptr};
    QLabel*              suggestionLabel_{nullptr};
};

} // namespace OpenC3::UI::Views
