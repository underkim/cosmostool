#include "PluginView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QFont>
#include <QFileDialog>
#include <QMessageBox>

namespace OpenC3::UI::Views {

PluginView::PluginView(ViewModels::PluginViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void PluginView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("Plugin Manager", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    refreshBtn_  = new QPushButton("↻  Refresh",  this);
    installBtn_  = new QPushButton("⬇  Install",  this);
    removeBtn_   = new QPushButton("✕  Remove",   this);
    validateBtn_ = new QPushButton("✔  Validate", this);
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(false);
    progressBar_->setFixedWidth(120);

    removeBtn_->setEnabled(false);

    toolbar->addWidget(refreshBtn_);
    toolbar->addSpacing(8);
    toolbar->addWidget(installBtn_);
    toolbar->addWidget(removeBtn_);
    toolbar->addWidget(validateBtn_);
    toolbar->addStretch();
    toolbar->addWidget(progressBar_);
    root->addLayout(toolbar);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    // ── Splitter: table / detail ──────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Vertical, this);

    tableView_ = new QTableView(splitter);
    tableView_->setModel(vm_.pluginModel());
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setShowGrid(false);
    tableView_->setSortingEnabled(true);

    auto* detailGroup  = new QGroupBox("Plugin Detail / Validation", splitter);
    auto* detailLayout = new QVBoxLayout(detailGroup);
    detailEdit_ = new QTextEdit(detailGroup);
    detailEdit_->setReadOnly(true);
    detailEdit_->setObjectName("LogArea");
    detailLayout->addWidget(detailEdit_);

    splitter->addWidget(tableView_);
    splitter->addWidget(detailGroup);
    splitter->setSizes({420, 200});
    root->addWidget(splitter);
}

void PluginView::bindViewModel()
{
    connect(refreshBtn_,  &QPushButton::clicked, &vm_, &ViewModels::PluginViewModel::refresh);
    connect(installBtn_,  &QPushButton::clicked, this, &PluginView::onInstallClicked);
    connect(removeBtn_,   &QPushButton::clicked, this, &PluginView::onRemoveClicked);
    connect(validateBtn_, &QPushButton::clicked, this, &PluginView::onValidateClicked);

    connect(tableView_->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, &PluginView::onTableSelectionChanged);

    connect(&vm_, &ViewModels::PluginViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy();
                progressBar_->setVisible(busy);
                refreshBtn_->setEnabled(!busy);
                installBtn_->setEnabled(!busy);
                validateBtn_->setEnabled(!busy);
                if (busy) removeBtn_->setEnabled(false);
            });

    connect(&vm_, &ViewModels::PluginViewModel::statusMessageChanged,
            this, [this] {
                statusLabel_->setText(vm_.statusMessage());
            });

    connect(&vm_, &ViewModels::PluginViewModel::validationComplete,
            this, [this](bool valid, const QString& summary) {
                detailEdit_->setPlainText(summary);
                if (!valid) {
                    QMessageBox::warning(this, "Validation",
                        "Plugin has validation issues:\n\n" + summary);
                }
            });

    connect(&vm_, &ViewModels::PluginViewModel::actionCompleted,
            this, [this](const QString& name, bool ok) {
                if (!ok)
                    QMessageBox::warning(this, "Plugin Manager",
                        "Action failed for: " + name);
            });
}

void PluginView::onInstallClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Plugin (.gem)", {}, "Gem files (*.gem);;All files (*)");
    if (!path.isEmpty())
        vm_.install(path);
}

void PluginView::onRemoveClicked()
{
    const QString name = selectedPluginName();
    if (name.isEmpty()) return;
    if (QMessageBox::question(this, "Confirm Remove",
            "Remove plugin: " + name + "?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        vm_.remove(name);
}

void PluginView::onValidateClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Plugin Directory or .gem", {}, "All files (*)");
    if (!path.isEmpty())
        vm_.validate(path);
}

void PluginView::onTableSelectionChanged()
{
    const bool hasSel = tableView_->selectionModel()->hasSelection();
    removeBtn_->setEnabled(hasSel && !vm_.isBusy());

    if (!hasSel) { detailEdit_->clear(); return; }

    const auto* plugin =
        vm_.pluginModel()->pluginAt(tableView_->selectionModel()->selectedRows().first().row());
    if (!plugin) return;

    const QString detail = QString(
        "Name:    %1\nVersion: %2\nAuthor:  %3\nCOSMOS:  %4\n\n%5\n\nTargets:\n%6")
        .arg(QString::fromStdString(plugin->name))
        .arg(QString::fromStdString(plugin->version))
        .arg(QString::fromStdString(plugin->author))
        .arg(QString::fromStdString(plugin->cosmosVersion))
        .arg(QString::fromStdString(plugin->description))
        .arg([&]{
            QStringList tgts;
            for (const auto& t : plugin->targets)
                tgts << QString("  %1 (%2)")
                    .arg(QString::fromStdString(t.name))
                    .arg(QString::fromStdString(t.interfaceType));
            return tgts.isEmpty() ? "  (none)" : tgts.join('\n');
        }());

    detailEdit_->setPlainText(detail);
}

QString PluginView::selectedPluginName() const
{
    const auto rows = tableView_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* p = vm_.pluginModel()->pluginAt(rows.first().row());
    return p ? QString::fromStdString(p->name) : QString{};
}

} // namespace OpenC3::UI::Views
