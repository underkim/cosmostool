#include "ScreenWidgetDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace OpenC3::UI::Dialogs {

namespace {
QString quoted(QString value)
{
    value.replace('"', '\'');
    return '"' + value + '"';
}

// Target/Packet/Item become bare (unquoted) space-separated tokens in the
// generated VALUE line, unlike Title/Label/Button text which is quoted - a
// name containing whitespace (e.g. "MY SAT") would silently split into
// extra tokens and corrupt the line, same risk the sibling
// PluginManifestInterfaceDialog/NewMicroserviceDialog already guard against
// for their own bare-token name fields.
bool isValidToken(const QString& value)
{
    static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return pattern.match(value).hasMatch();
}
} // namespace

ScreenWidgetDialog::ScreenWidgetDialog(const QStringList& knownTargets, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Widget"));
    setModal(true);
    resize(380, 220);

    auto* root = new QVBoxLayout(this);

    kindCombo_ = new QComboBox(this);
    kindCombo_->addItems({tr("Title"), tr("Label"), tr("Value (telemetry)"), tr("Button")});
    auto* kindForm = new QFormLayout;
    kindForm->addRow(tr("Widget:"), kindCombo_);
    root->addLayout(kindForm);

    fieldStack_ = new QStackedWidget(this);

    // ── Title / Label page (shared - both are just display text) ────────────
    auto* textPage = new QWidget(fieldStack_);
    auto* textForm = new QFormLayout(textPage);
    textEdit_ = new QLineEdit(textPage);
    textEdit_->setPlaceholderText(tr("Text to display"));
    textForm->addRow(tr("Text:"), textEdit_);
    fieldStack_->addWidget(textPage); // index 0 (Title), reused for Label too

    // ── Value page ────────────────────────────────────────────────────────────
    auto* valuePage = new QWidget(fieldStack_);
    auto* valueForm = new QFormLayout(valuePage);
    targetCombo_ = new QComboBox(valuePage);
    targetCombo_->setEditable(true);
    targetCombo_->addItems(knownTargets);
    if (knownTargets.isEmpty())
        targetCombo_->setPlaceholderText(tr("e.g. MYSAT"));
    packetEdit_ = new QLineEdit(valuePage);
    packetEdit_->setPlaceholderText(tr("e.g. STATUS"));
    itemEdit_ = new QLineEdit(valuePage);
    itemEdit_->setPlaceholderText(tr("e.g. TEMPERATURE"));
    valueForm->addRow(tr("Target:"), targetCombo_);
    valueForm->addRow(tr("Packet:"), packetEdit_);
    valueForm->addRow(tr("Item:"), itemEdit_);
    fieldStack_->addWidget(valuePage); // index 1

    // ── Button page ───────────────────────────────────────────────────────────
    auto* buttonPage = new QWidget(fieldStack_);
    auto* buttonForm = new QFormLayout(buttonPage);
    buttonLabelEdit_ = new QLineEdit(buttonPage);
    buttonLabelEdit_->setPlaceholderText(tr("e.g. Send Reset"));
    buttonCommandEdit_ = new QLineEdit(buttonPage);
    buttonCommandEdit_->setPlaceholderText(tr("e.g. cmd(\"MYSAT RESET\")"));
    buttonForm->addRow(tr("Label:"), buttonLabelEdit_);
    buttonForm->addRow(tr("On click, run:"), buttonCommandEdit_);
    fieldStack_->addWidget(buttonPage); // index 2

    root->addWidget(fieldStack_);

    connect(kindCombo_, &QComboBox::currentIndexChanged,
            this, &ScreenWidgetDialog::updateFieldsForKind);
    updateFieldsForKind(0);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const int kind = kindCombo_->currentIndex();
        if (kind == 0 || kind == 1) { // Title / Label
            if (textEdit_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Add Widget"), tr("Enter the text to display."));
                return;
            }
        } else if (kind == 2) { // Value
            if (targetCombo_->currentText().trimmed().isEmpty()
                || packetEdit_->text().trimmed().isEmpty()
                || itemEdit_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Add Widget"),
                    tr("Target, Packet, and Item are all required."));
                return;
            }
            if (!isValidToken(targetCombo_->currentText().trimmed())
                || !isValidToken(packetEdit_->text().trimmed())
                || !isValidToken(itemEdit_->text().trimmed())) {
                QMessageBox::warning(this, tr("Add Widget"),
                    tr("Target, Packet, and Item must start with a letter or "
                       "underscore and contain only letters, numbers, and "
                       "underscores (no spaces or punctuation)."));
                return;
            }
        } else { // Button
            if (buttonLabelEdit_->text().trimmed().isEmpty()
                || buttonCommandEdit_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Add Widget"),
                    tr("Label and the command to run are both required."));
                return;
            }
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ScreenWidgetDialog::updateFieldsForKind(int kindIndex)
{
    // Title (0) and Label (1) share the same text page; Value (2) and
    // Button (3) each have their own.
    fieldStack_->setCurrentIndex(kindIndex == 0 || kindIndex == 1 ? 0 : kindIndex - 1);
}

QString ScreenWidgetDialog::generatedLine() const
{
    switch (kindCombo_->currentIndex()) {
    case 0: // Title
        return QString("TITLE %1\n").arg(quoted(textEdit_->text().trimmed()));
    case 1: // Label
        return QString("LABEL %1\n").arg(quoted(textEdit_->text().trimmed()));
    case 2: // Value
        return QString("VALUE %1 %2 %3\n")
            .arg(targetCombo_->currentText().trimmed().toUpper(),
                 packetEdit_->text().trimmed().toUpper(),
                 itemEdit_->text().trimmed().toUpper());
    default: // Button
        return QString("BUTTON %1 %2\n")
            .arg(quoted(buttonLabelEdit_->text().trimmed()),
                 quoted(buttonCommandEdit_->text().trimmed()));
    }
}

} // namespace OpenC3::UI::Dialogs
