#include "ValidatorView.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace OpenC3::UI::Views {

using Severity = ViewModels::Validation::Diagnostic::Severity;

namespace {

QString severityPrefix(Severity severity)
{
    switch (severity) {
    case Severity::Error:   return QStringLiteral("Error");
    case Severity::Warning: return QStringLiteral("Warning");
    case Severity::Info:    return QStringLiteral("Info");
    }
    return QStringLiteral("Status");
}

QString prefixedDiagnosticMessage(Severity severity, const QString& message)
{
    const QString prefix = severityPrefix(severity);
    if (message == prefix || message.startsWith(prefix + QStringLiteral(":")))
        return message;
    return QStringLiteral("%1: %2").arg(prefix, message);
}

} // namespace

ValidatorView::ValidatorView(ViewModels::ValidatorViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
}

void ValidatorView::setupUi()
{
    auto* root = new QVBoxLayout(this);

    auto* title = new QLabel(tr("Configuration Validator"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Offline static checks for COSMOS CMD/TLM, screens, and plugin.txt. "
           "No COSMOS runtime required."),
        this);
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout();
    folderBtn_ = new QPushButton(tr("Validate Folder…"), this);
    fileBtn_   = new QPushButton(tr("Validate File…"), this);
    toolbar->addWidget(folderBtn_);
    toolbar->addWidget(fileBtn_);
    toolbar->addStretch(1);
    summaryLabel_ = new QLabel(this);
    toolbar->addWidget(summaryLabel_);
    root->addLayout(toolbar);

    // ── Paste & check ────────────────────────────────────────────────────────
    auto* pasteGroup = new QGroupBox(tr("Paste && Check"), this);
    auto* pasteLayout = new QVBoxLayout(pasteGroup);

    auto* pasteRow = new QHBoxLayout();
    pasteRow->addWidget(new QLabel(tr("Type:"), this));
    kindCombo_ = new QComboBox(this);
    kindCombo_->addItem(tr("Auto-detect"), QString());
    for (const auto& v : vm_.availableValidators())
        kindCombo_->addItem(v.second, v.first); // label, id
    pasteRow->addWidget(kindCombo_);
    checkBtn_ = new QPushButton(tr("Check"), this);
    pasteRow->addWidget(checkBtn_);
    pasteRow->addStretch(1);
    pasteLayout->addLayout(pasteRow);

    pasteEdit_ = new QPlainTextEdit(this);
    pasteEdit_->setPlaceholderText(
        tr("Paste a COSMOS configuration snippet here…"));
    pasteEdit_->setMaximumHeight(140);
    pasteLayout->addWidget(pasteEdit_);

    root->addWidget(pasteGroup);

    // ── Results ──────────────────────────────────────────────────────────────
    resultsTree_ = new QTreeWidget(this);
    resultsTree_->setColumnCount(3);
    resultsTree_->setHeaderLabels({ tr("Severity"), tr("Line"), tr("Message") });
    resultsTree_->setRootIsDecorated(true);
    resultsTree_->setUniformRowHeights(true);
    resultsTree_->setColumnWidth(0, 90);
    resultsTree_->setColumnWidth(1, 60);
    resultsTree_->header()->setStretchLastSection(true);
    root->addWidget(resultsTree_, 1);

    detailLabel_ = new QLabel(this);
    detailLabel_->setWordWrap(true);
    detailLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(detailLabel_);
}

void ValidatorView::bindViewModel()
{
    connect(folderBtn_, &QPushButton::clicked, this, &ValidatorView::onValidateFolder);
    connect(fileBtn_,   &QPushButton::clicked, this, &ValidatorView::onValidateFile);
    connect(checkBtn_,  &QPushButton::clicked, this, &ValidatorView::onCheckPasted);
    connect(&vm_, &ViewModels::ValidatorViewModel::reportReady,
            this, &ValidatorView::onReportReady);
    connect(resultsTree_, &QTreeWidget::currentItemChanged,
            this, &ValidatorView::onSelectionChanged);
}

void ValidatorView::onValidateFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select a plugin or target folder"));
    if (!dir.isEmpty())
        vm_.validateFolder(dir);
}

void ValidatorView::onValidateFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select a config file"), QString(),
        tr("Config files (*.txt);;All files (*)"));
    if (!path.isEmpty())
        vm_.validateFile(path);
}

void ValidatorView::checkContent(const QString& content)
{
    if (content.trimmed().isEmpty())
        return;
    kindCombo_->setCurrentIndex(0);          // Auto-detect
    pasteEdit_->setPlainText(content);
    vm_.checkContent(content);
}

void ValidatorView::onCheckPasted()
{
    const QString text = pasteEdit_->toPlainText();
    if (text.trimmed().isEmpty())
        return;
    vm_.validateTextWith(kindCombo_->currentData().toString(), text);
}

void ValidatorView::onReportReady()
{
    resultsTree_->clear();
    detailLabel_->clear();

    const auto& report = vm_.report();
    summaryLabel_->setText(QStringLiteral("%1  —  %2")
                               .arg(vm_.lastSource(), report.summary()));

    if (report.diagnostics.isEmpty()) {
        auto* ok = new QTreeWidgetItem(resultsTree_);
        ok->setText(0, tr("OK"));
        ok->setText(2, tr("No problems found."));
        return;
    }

    QHash<QString, QTreeWidgetItem*> groups;

    for (const auto& d : report.diagnostics) {
        const QString file = d.filePath.isEmpty() ? vm_.lastSource() : d.filePath;

        QTreeWidgetItem* group = groups.value(file, nullptr);
        if (!group) {
            group = new QTreeWidgetItem(resultsTree_);
            group->setFirstColumnSpanned(true);
            QFont gf = group->font(2);
            gf.setBold(true);
            group->setFont(2, gf);
            group->setText(2, file);
            group->setExpanded(true);
            groups.insert(file, group);
        }

        QIcon  icon;
        QBrush brush;
        switch (d.severity) {
        case Severity::Error:
            icon  = style()->standardIcon(QStyle::SP_MessageBoxCritical);
            brush = QBrush(QColor(0xE5, 0x48, 0x48));
            break;
        case Severity::Warning:
            icon  = style()->standardIcon(QStyle::SP_MessageBoxWarning);
            brush = QBrush(QColor(0xE0, 0x9B, 0x2D));
            break;
        case Severity::Info:
            icon  = style()->standardIcon(QStyle::SP_MessageBoxInformation);
            brush = QBrush(QColor(0x9A, 0x9A, 0x9A));
            break;
        }

        auto* item = new QTreeWidgetItem(group);
        item->setIcon(0, icon);
        item->setText(0, d.severityLabel());
        item->setForeground(0, brush);
        item->setText(1, d.line > 0 ? QString::number(d.line) : QString());
        item->setText(2, prefixedDiagnosticMessage(d.severity, d.message));
        item->setData(0, Qt::UserRole,     d.suggestion);
        item->setData(0, Qt::UserRole + 1, d.rule);
    }
}

void ValidatorView::onSelectionChanged()
{
    QTreeWidgetItem* item = resultsTree_->currentItem();
    if (!item) {
        detailLabel_->clear();
        return;
    }

    const QString suggestion = item->data(0, Qt::UserRole).toString();
    const QString rule       = item->data(0, Qt::UserRole + 1).toString();

    QStringList parts;
    if (!rule.isEmpty())
        parts << tr("Rule: %1").arg(rule);
    if (!suggestion.isEmpty())
        parts << tr("Suggestion: %1").arg(suggestion);
    detailLabel_->setText(parts.join(QStringLiteral("     ")));
}

} // namespace OpenC3::UI::Views
