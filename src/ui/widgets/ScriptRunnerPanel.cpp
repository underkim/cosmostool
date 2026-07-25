#include "ScriptRunnerPanel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace OpenC3::UI::Widgets {

ScriptRunnerPanel::ScriptRunnerPanel(
    ViewModels::ScriptViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    refreshBtn_ = new QPushButton(tr("Refresh Scripts"), this);
    refreshBtn_->setToolTip(tr(
        "List scripts actually deployed in the connected scope's storage "
        "(requires an installed plugin - not the local Workspace source)."));

    runBtn_ = new QPushButton(tr("Run in COSMOS"), this);
    runBtn_->setEnabled(false);
    runBtn_->setToolTip(tr(
        "Starts the selected script inside COSMOS Script Runner, disconnected "
        "(no interactive prompt/breakpoint support in this client)."));

    stopBtn_   = new QPushButton(tr("Stop"), this);
    pauseBtn_  = new QPushButton(tr("Pause"), this);
    resumeBtn_ = new QPushButton(tr("Resume"), this);
    stopBtn_->setEnabled(false);
    pauseBtn_->setEnabled(false);
    resumeBtn_->setEnabled(false);

    statusLabel_ = new QLabel(tr("No active script run."), this);
    statusLabel_->setObjectName("ScriptRunStatus");
    statusLabel_->setWordWrap(true);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    listView_ = new QListView(this);
    listView_->setObjectName("CosmosScriptList");
    listView_->setModel(vm_.scriptModel());
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    listView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView_->setMinimumHeight(150);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(refreshBtn_);
    toolbar->addWidget(runBtn_);
    toolbar->addWidget(stopBtn_);
    toolbar->addWidget(pauseBtn_);
    toolbar->addWidget(resumeBtn_);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(statusLabel_);
    layout->addWidget(listView_);

    connect(refreshBtn_, &QPushButton::clicked,
            this, [this] { vm_.refresh(); });
    connect(runBtn_, &QPushButton::clicked,
            this, &ScriptRunnerPanel::onRunClicked);
    connect(stopBtn_, &QPushButton::clicked, &vm_, &ViewModels::ScriptViewModel::stop);
    connect(pauseBtn_, &QPushButton::clicked, &vm_, &ViewModels::ScriptViewModel::pause);
    connect(resumeBtn_, &QPushButton::clicked, &vm_, &ViewModels::ScriptViewModel::resume);

    connect(listView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ScriptRunnerPanel::onSelectionOrBusyChanged);
    connect(&vm_, &ViewModels::ScriptViewModel::busyChanged,
            this, &ScriptRunnerPanel::onSelectionOrBusyChanged);

    connect(&vm_, &ViewModels::ScriptViewModel::scriptStatusChanged,
            this, [this](const QString& id, const QString& state,
                         const QString& detail, bool finished, bool successful) {
                QString heading;
                if (!finished)
                    heading = tr("Script running");
                else if (successful)
                    heading = tr("Script completed");
                else
                    heading = tr("Script ended");

                QString text = tr("%1: %2\nRunning-script id: %3")
                                   .arg(heading, state, id);
                if (!detail.trimmed().isEmpty()) text += "\n" + detail.trimmed();
                statusLabel_->setText(text);
                statusLabel_->setToolTip(text);
                onSelectionOrBusyChanged();
            });
}

void ScriptRunnerPanel::onSelectionOrBusyChanged()
{
    const bool busy       = vm_.isBusy();
    const bool hasRun     = !vm_.activeRunningScriptId().isEmpty();
    const bool hasSel     = listView_->selectionModel()->hasSelection();

    refreshBtn_->setEnabled(!busy);
    runBtn_->setEnabled(!busy && hasSel);
    stopBtn_->setEnabled(hasRun);
    pauseBtn_->setEnabled(hasRun);
    resumeBtn_->setEnabled(hasRun);
}

void ScriptRunnerPanel::onRunClicked()
{
    const auto rows = listView_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;

    const auto* script = vm_.scriptModel()->scriptAt(rows.first().row());
    if (!script) return;
    const QString path = QString::fromStdString(script->path);

    if (QMessageBox::question(this, tr("Run Script"),
            tr("Start this script inside COSMOS Script Runner "
               "(disconnected mode - no interactive prompts)?\n\n%1")
                .arg(path),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        vm_.run(path);
    }
}

} // namespace OpenC3::UI::Widgets
