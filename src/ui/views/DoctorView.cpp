#include "DoctorView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QLabel>
#include <QFont>

namespace OpenC3::UI::Views {

DoctorView::DoctorView(
    ViewModels::DoctorViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void DoctorView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("System Doctor", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Connection hint ───────────────────────────────────────────────────────
    auto* hint = new QLabel(
        "Checks run on the connected environment - connect first "
        "(Home > Connect) for meaningful results.", this);
    hint->setObjectName("SubLabel");
    hint->setWordWrap(true);
    root->addWidget(hint);

    // ── Action row ────────────────────────────────────────────────────────────
    auto* actionRow = new QHBoxLayout;
    runBtn_     = new QPushButton("Run Health Checks", this);
    runBtn_->setFixedHeight(36);
    runBtn_->setObjectName("PrimaryButton");
    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 0); // indeterminate

    actionRow->addWidget(runBtn_);
    actionRow->addWidget(progressBar_);
    actionRow->addStretch();
    root->addLayout(actionRow);

    // ── Summary ───────────────────────────────────────────────────────────────
    summaryLabel_ = new QLabel("Run checks to see results.", this);
    summaryLabel_->setObjectName("SubLabel");
    root->addWidget(summaryLabel_);

    // ── Results table ─────────────────────────────────────────────────────────
    tableView_ = new QTableView(this);
    tableView_->setModel(vm_.resultsModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setShowGrid(false);
    root->addWidget(tableView_);

    // ── Suggestion panel ──────────────────────────────────────────────────────
    auto* suggGroup  = new QGroupBox("Suggestion", this);
    auto* suggLayout = new QVBoxLayout(suggGroup);
    suggestionLabel_ = new QLabel(this);
    suggestionLabel_->setWordWrap(true);
    suggestionLabel_->setObjectName("SubLabel");
    suggLayout->addWidget(suggestionLabel_);
    // Minimum (not fixed) so long suggestions wrap and grow instead of clipping.
    suggGroup->setMinimumHeight(80);
    root->addWidget(suggGroup);
}

void DoctorView::bindViewModel()
{
    connect(runBtn_, &QPushButton::clicked, this, [this] {
        runBtn_->setEnabled(false);
        progressBar_->setVisible(true);
        vm_.runAllChecks();
    });

    connect(&vm_, &ViewModels::DoctorViewModel::checkProgressUpdated,
            this, [this](const QString& name, const QString& status) {
                summaryLabel_->setText(
                    QString("Checking: %1  [%2]").arg(name, status));
            });

    connect(&vm_, &ViewModels::DoctorViewModel::summaryChanged,
            this, [this] {
                progressBar_->setVisible(false);
                runBtn_->setEnabled(true);
                summaryLabel_->setText(
                    QString("%1 passed, %2 warnings, %3 failed")
                        .arg(vm_.passCount())
                        .arg(vm_.warningCount())
                        .arg(vm_.failCount()));
            });

    // Show suggestion when user selects a row
    connect(tableView_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& idx) {
                const auto* item = qobject_cast<
                    ViewModels::HealthCheckTableModel*>(
                        tableView_->model());
                if (!item) return;
                const QVariant tip = item->data(
                    item->index(idx.row(),
                        ViewModels::HealthCheckTableModel::Column::Detail),
                    Qt::ToolTipRole);
                suggestionLabel_->setText(tip.toString());
            });
}

} // namespace OpenC3::UI::Views
