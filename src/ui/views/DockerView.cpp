#include "DockerView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QFont>

namespace OpenC3::UI::Views {

DockerView::DockerView(
    ViewModels::DockerViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void DockerView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title + meta ──────────────────────────────────────────────────────────
    auto* header = new QHBoxLayout;
    auto* title  = new QLabel("Docker Manager", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    versionLabel_ = new QLabel("--", this);
    versionLabel_->setObjectName("SubLabel");
    header->addWidget(title);
    header->addStretch();
    header->addWidget(versionLabel_);
    root->addLayout(header);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    refreshBtn_   = new QPushButton("↻  Refresh", this);
    startBtn_     = new QPushButton("▶  Start",   this);
    stopBtn_      = new QPushButton("■  Stop",    this);
    restartBtn_   = new QPushButton("↺  Restart", this);
    removeBtn_    = new QPushButton("✕  Remove",  this);
    logsBtn_      = new QPushButton("📋  Logs",   this);

    for (auto* btn : {startBtn_, stopBtn_, restartBtn_, removeBtn_, logsBtn_})
        btn->setEnabled(false);

    toolbar->addWidget(refreshBtn_);
    toolbar->addSpacing(8);
    toolbar->addWidget(startBtn_);
    toolbar->addWidget(stopBtn_);
    toolbar->addWidget(restartBtn_);
    toolbar->addWidget(removeBtn_);
    toolbar->addWidget(logsBtn_);
    toolbar->addStretch();
    root->addLayout(toolbar);

    // ── Splitter: table / log ─────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Vertical, this);

    tableView_ = new QTableView(splitter);
    tableView_->setModel(vm_.containerModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setShowGrid(false);
    tableView_->setSortingEnabled(true);

    auto* logGroup = new QGroupBox("Container Logs", splitter);
    auto* logLayout = new QVBoxLayout(logGroup);
    logWidget_ = new Widgets::LogWidget(logGroup);
    logLayout->addWidget(logWidget_);

    splitter->addWidget(tableView_);
    splitter->addWidget(logGroup);
    splitter->setChildrenCollapsible(false); // keep table and logs visible on drag
    splitter->setStretchFactor(0, 1);
    tableView_->setMinimumHeight(120);
    logGroup->setMinimumHeight(120);
    splitter->setSizes({420, 280});
    root->addWidget(splitter);
}

void DockerView::bindViewModel()
{
    connect(refreshBtn_, &QPushButton::clicked, &vm_, &ViewModels::DockerViewModel::refresh);

    connect(startBtn_,   &QPushButton::clicked, this, &DockerView::onStartClicked);
    connect(stopBtn_,    &QPushButton::clicked, this, &DockerView::onStopClicked);
    connect(restartBtn_, &QPushButton::clicked, this, &DockerView::onRestartClicked);
    connect(removeBtn_,  &QPushButton::clicked, this, &DockerView::onRemoveClicked);
    connect(logsBtn_,    &QPushButton::clicked, this, &DockerView::onViewLogsClicked);

    connect(tableView_->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, &DockerView::onTableSelectionChanged);

    connect(&vm_, &ViewModels::DockerViewModel::dockerVersionChanged,
            this, [this] {
                versionLabel_->setText("Docker " + vm_.dockerVersion());
            });

    connect(&vm_, &ViewModels::DockerViewModel::logsReady,
            this, [this](const QString& /*id*/, const QString& logs) {
                logWidget_->setContent(logs);
            });

    connect(&vm_, &ViewModels::DockerViewModel::containerActionCompleted,
            this, [this](const QString& name, bool ok) {
                if (!ok) {
                    QMessageBox::warning(this, "Docker",
                        "Action failed for: " + name);
                }
            });
}

void DockerView::onTableSelectionChanged()
{
    const bool hasSelection = tableView_->selectionModel()->hasSelection();
    startBtn_->setEnabled(hasSelection);
    stopBtn_->setEnabled(hasSelection);
    restartBtn_->setEnabled(hasSelection);
    removeBtn_->setEnabled(hasSelection);
    logsBtn_->setEnabled(hasSelection);
}

void DockerView::onStartClicked()
{
    vm_.startContainer(selectedContainerName());
}

void DockerView::onStopClicked()
{
    vm_.stopContainer(selectedContainerName());
}

void DockerView::onRestartClicked()
{
    vm_.restartContainer(selectedContainerName());
}

void DockerView::onRemoveClicked()
{
    const QString name = selectedContainerName();
    if (QMessageBox::question(this, "Confirm Remove",
            "Remove container: " + name + "?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        vm_.removeContainer(name);
}

void DockerView::onViewLogsClicked()
{
    vm_.fetchLogs(selectedContainerName());
}

QString DockerView::selectedContainerName() const
{
    const auto indexes = tableView_->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return {};
    const auto* c = vm_.containerModel()->containerAt(indexes[0].row());
    return c ? QString::fromStdString(c->name) : QString{};
}

} // namespace OpenC3::UI::Views
