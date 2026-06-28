#include "CmdTlmFieldDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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
    setWindowTitle("Add CMD/TLM Field");
    setModal(true);
    resize(420, 360);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem("Command parameter", "parameter");
    modeCombo_->addItem("Telemetry item", "item");

    idFieldCheck_ = new QCheckBox("Identifier field", this);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("TEMP, CMD_ID, COUNTER");

    bitSizeSpin_ = new QSpinBox(this);
    bitSizeSpin_->setRange(1, 65535);
    bitSizeSpin_->setValue(16);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItems({
        "UINT", "INT", "FLOAT",
        "UINT8", "UINT16", "UINT32", "UINT64",
        "INT8", "INT16", "INT32", "INT64",
        "FLOAT32", "FLOAT64",
        "STRING", "BLOCK", "DERIVED"
    });

    minEdit_ = new QLineEdit("0", this);
    maxEdit_ = new QLineEdit("65535", this);
    defaultEdit_ = new QLineEdit("0", this);
    descriptionEdit_ = new QLineEdit(this);
    descriptionEdit_->setPlaceholderText("Short field description");

    form->addRow("Kind", modeCombo_);
    form->addRow("", idFieldCheck_);
    form->addRow("Name", nameEdit_);
    form->addRow("Bit size", bitSizeSpin_);
    form->addRow("Type", typeCombo_);
    form->addRow("Minimum", minEdit_);
    form->addRow("Maximum", maxEdit_);
    form->addRow("Default", defaultEdit_);
    form->addRow("Description", descriptionEdit_);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(modeCombo_, &QComboBox::currentIndexChanged,
            this, [this] { updateMode(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (nameEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Add Field", "Name is required.");
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
        return QString("  %1 %2 %3 %4 %5 %6 %7 %8\n")
            .arg(keyword, name)
            .arg(bitSizeSpin_->value())
            .arg(type, minEdit_->text().trimmed(), maxEdit_->text().trimmed(),
                 defaultEdit_->text().trimmed(), desc);
    }

    return QString("  %1 %2 %3 %4 %5\n")
        .arg(keyword, name)
        .arg(bitSizeSpin_->value())
        .arg(type, desc);
}

void CmdTlmFieldDialog::updateMode()
{
    const bool parameter = modeCombo_->currentData().toString() == "parameter";
    minEdit_->setEnabled(parameter);
    maxEdit_->setEnabled(parameter);
    defaultEdit_->setEnabled(parameter);
}

} // namespace OpenC3::UI::Dialogs
