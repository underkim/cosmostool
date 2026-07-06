#include "CmdTlmFieldDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

namespace OpenC3::UI::Dialogs {

namespace {

QString quotedDescription(QString value)
{
    value.replace('"', '\'');
    return '"' + value + '"';
}

} // namespace

CmdTlmFieldDialog::CmdTlmFieldDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add CMD/TLM Field"));
    setModal(true);
    resize(420, 360);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem(tr("Command parameter"), "parameter");
    modeCombo_->addItem(tr("Telemetry item"), "item");

    idFieldCheck_ = new QCheckBox(tr("Identifier field"), this);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("TEMP, CMD_ID, COUNTER");

    bitSizeSpin_ = new QSpinBox(this);
    bitSizeSpin_->setRange(1, 65535);
    bitSizeSpin_->setValue(16);
    // Unlike every other narrow numeric field in this app (e.g. SSH Port,
    // Interface Port), this had no width cap, so it stretched to fill the
    // whole form-field column - a lone oversized spinner next to otherwise
    // compact rows.
    bitSizeSpin_->setMaximumWidth(100);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItems({
        "UINT", "INT", "FLOAT",
        "UINT8", "UINT16", "UINT32", "UINT64",
        "INT8", "INT16", "INT32", "INT64",
        "FLOAT32", "FLOAT64",
        "STRING", "BLOCK", "DERIVED"
    });

    // Free text rather than a numeric validator: COSMOS min/max/default
    // commonly hold hex literals (0x01) or the MIN/MAX keywords, not just
    // plain decimals, so a strict QDoubleValidator would reject valid input.
    minEdit_ = new QLineEdit("0", this);
    maxEdit_ = new QLineEdit("65535", this);
    defaultEdit_ = new QLineEdit("0", this);
    descriptionEdit_ = new QLineEdit(this);
    descriptionEdit_->setPlaceholderText(tr("Short field description"));

    form->addRow(tr("Kind"), modeCombo_);
    form->addRow("", idFieldCheck_);
    form->addRow(tr("Name"), nameEdit_);
    form->addRow(tr("Bit size"), bitSizeSpin_);
    form->addRow(tr("Type"), typeCombo_);
    form->addRow(tr("Minimum"), minEdit_);
    form->addRow(tr("Maximum"), maxEdit_);
    form->addRow(tr("Default / ID Value"), defaultEdit_);
    form->addRow(tr("Description"), descriptionEdit_);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(modeCombo_, &QComboBox::currentIndexChanged,
            this, [this] { updateMode(); });
    connect(typeCombo_, &QComboBox::currentIndexChanged,
            this, [this] { updateMode(); });
    connect(idFieldCheck_, &QCheckBox::toggled,
            this, [this] { updateMode(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const QString name = nameEdit_->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, tr("Add Field"), tr("Name is required."));
            return;
        }
        // The generated line is whitespace-tokenized when re-parsed (see
        // CmdTlmParser), so a name containing a space or other COSMOS
        // syntax character (quotes, etc.) would silently shift every field
        // after it instead of failing loudly here.
        static const QRegularExpression validName("^[A-Za-z_][A-Za-z0-9_]*$");
        if (!validName.match(name).hasMatch()) {
            QMessageBox::warning(this, tr("Add Field"),
                tr("Name must start with a letter or underscore and contain only "
                   "letters, numbers, and underscores (no spaces or punctuation)."));
            return;
        }

        const bool parameter = modeCombo_->currentData().toString() == "parameter";
        const QString type   = typeCombo_->currentText();
        // Matches CmdTlmParser's own strBlock check: only STRING/BLOCK omit the
        // MIN/MAX positional tokens for APPEND_PARAMETER. DERIVED still expects
        // them, so it must not be lumped in here (see generatedLine() below).
        const bool strBlock  = (type == "STRING" || type == "BLOCK");
        if (parameter && !strBlock) {
            if (minEdit_->text().trimmed().isEmpty()
                || maxEdit_->text().trimmed().isEmpty()
                || defaultEdit_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Add Field"),
                    tr("Minimum, Maximum, and Default are required for this type."));
                return;
            }
            // Only compare when both parse as plain numbers - COSMOS min/max
            // may also hold hex literals (0x01) or MIN/MAX keywords, which
            // this check intentionally does not reject.
            bool minOk = false, maxOk = false;
            const double minVal = minEdit_->text().trimmed().toDouble(&minOk);
            const double maxVal = maxEdit_->text().trimmed().toDouble(&maxOk);
            if (minOk && maxOk && minVal > maxVal) {
                QMessageBox::warning(this, tr("Add Field"),
                    tr("Minimum must not be greater than Maximum."));
                return;
            }
        }
        // Telemetry item's APPEND_ID_ITEM needs its own id_value token
        // (unlike plain APPEND_ITEM) - see generatedLine() below. Without
        // one, CmdTlmParser's own column layout for ID items shifts the
        // description into the id_value slot instead of failing loudly.
        if (!parameter && idFieldCheck_->isChecked()
            && defaultEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Add Field"),
                tr("An ID value is required for identifier fields."));
            return;
        }

        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateMode();

    nameEdit_->setFocus(); // keyboard lands on the Name field
}

QString CmdTlmFieldDialog::generatedLine() const
{
    const bool parameter = modeCombo_->currentData().toString() == "parameter";
    const QString keyword = parameter
        ? (idFieldCheck_->isChecked() ? "APPEND_ID_PARAMETER" : "APPEND_PARAMETER")
        : (idFieldCheck_->isChecked() ? "APPEND_ID_ITEM" : "APPEND_ITEM");

    const QString name = nameEdit_->text().trimmed().toUpper();
    const QString type = typeCombo_->currentText();
    const QString desc = quotedDescription(descriptionEdit_->text().trimmed());

    if (parameter) {
        // Must match CmdTlmParser's own strBlock check (STRING/BLOCK only):
        // those types have no MIN/MAX slots in APPEND_PARAMETER, only
        // <NAME> <BIT_SIZE> <TYPE> <DEFAULT> "description". Emitting MIN/MAX
        // tokens anyway would shift every field after them and silently
        // corrupt the description when the line is re-parsed.
        const bool strBlock = (type == "STRING" || type == "BLOCK");
        if (strBlock) {
            return QString("  %1 %2 %3 %4 %5 %6\n")
                .arg(keyword, name)
                .arg(bitSizeSpin_->value())
                .arg(type, defaultEdit_->text().trimmed(), desc);
        }
        return QString("  %1 %2 %3 %4 %5 %6 %7 %8\n")
            .arg(keyword, name)
            .arg(bitSizeSpin_->value())
            .arg(type, minEdit_->text().trimmed(), maxEdit_->text().trimmed(),
                 defaultEdit_->text().trimmed(), desc);
    }

    // APPEND_ID_ITEM inserts an id_value token right after the type, before
    // the description (see CmdTlmParser's own column layout) - plain
    // APPEND_ITEM has no such slot. Omitting it for an ID item would shift
    // the description into where the parser expects the id_value instead.
    if (idFieldCheck_->isChecked()) {
        return QString("  %1 %2 %3 %4 %5 %6\n")
            .arg(keyword, name)
            .arg(bitSizeSpin_->value())
            .arg(type, defaultEdit_->text().trimmed(), desc);
    }
    return QString("  %1 %2 %3 %4 %5\n")
        .arg(keyword, name)
        .arg(bitSizeSpin_->value())
        .arg(type, desc);
}

void CmdTlmFieldDialog::updateMode()
{
    const bool parameter = modeCombo_->currentData().toString() == "parameter";
    const QString type   = typeCombo_->currentText();
    const bool strBlock  = (type == "STRING" || type == "BLOCK");

    minEdit_->setEnabled(parameter && !strBlock);
    maxEdit_->setEnabled(parameter && !strBlock);
    // A telemetry item has no Default of its own, but an ID item still
    // needs this field to supply its id_value (see generatedLine()).
    defaultEdit_->setEnabled(parameter || idFieldCheck_->isChecked());
}

} // namespace OpenC3::UI::Dialogs
