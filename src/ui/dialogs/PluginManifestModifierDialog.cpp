#include "PluginManifestModifierDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringList>
#include <QVBoxLayout>

namespace OpenC3::UI::Dialogs {

namespace {
// Keyword is a single bare token at the start of the generated line -
// unlike Arguments (intentionally freeform/space-separated) - so a
// keyword containing whitespace would shift every argument after it by
// one position on reparse, same risk already guarded against in the
// sibling PluginManifestInterfaceDialog/NewMicroserviceDialog/
// ScreenWidgetDialog for their own bare-token fields.
bool isValidToken(const QString& value)
{
    static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return pattern.match(value).hasMatch();
}
} // namespace

PluginManifestModifierDialog::PluginManifestModifierDialog(
    const QSet<QString>& suggestedKeywords, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Modifier Line"));
    setModal(true);
    resize(380, 140);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    keywordCombo_ = new QComboBox(this);
    keywordCombo_->setEditable(true);
    QStringList sorted(suggestedKeywords.begin(), suggestedKeywords.end());
    sorted.sort();
    keywordCombo_->addItems(sorted);
    keywordCombo_->setCurrentText(sorted.isEmpty() ? QString() : sorted.first());

    argsEdit_ = new QLineEdit(this);
    argsEdit_->setPlaceholderText(tr("Arguments, e.g. READ_WRITE PreidentifiedProtocol"));

    form->addRow(tr("Keyword"), keywordCombo_);
    form->addRow(tr("Arguments"), argsEdit_);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (keywordCombo_->currentText().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Add Modifier Line"), tr("Keyword is required."));
            return;
        }
        if (!isValidToken(keywordCombo_->currentText().trimmed())) {
            QMessageBox::warning(this, tr("Add Modifier Line"),
                tr("Keyword must start with a letter or underscore and contain "
                   "only letters, numbers, and underscores (no spaces or punctuation)."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString PluginManifestModifierDialog::generatedLine() const
{
    const QString keyword = keywordCombo_->currentText().trimmed().toUpper();
    const QString args = argsEdit_->text().trimmed();
    return args.isEmpty()
        ? QString("  %1\n").arg(keyword)
        : QString("  %1 %2\n").arg(keyword, args);
}

} // namespace OpenC3::UI::Dialogs
