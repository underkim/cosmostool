#include "AddTargetDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFont>

namespace OpenC3::UI::Dialogs {

AddTargetDialog::AddTargetDialog(
    ViewModels::InfraViewModel& vm,
    const QString&              pluginRoot,
    QWidget*                    parent)
    : QDialog(parent)
    , vm_(vm)
    , pluginRoot_(pluginRoot)
{
    setWindowTitle("Add Target");
    setMinimumSize(480, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto* title = new QLabel(
        QString("Add a target to the plugin:\n<small>%1</small>").arg(pluginRoot), this);
    title->setTextFormat(Qt::RichText);
    title->setWordWrap(true);
    root->addWidget(title);

    // ── Form ──────────────────────────────────────────────────────────────────
    auto* grp  = new QGroupBox("Target Info", this);
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    targetNameEdit_ = new QLineEdit("NEWTARGET", grp);
    targetNameEdit_->setPlaceholderText("UPPERCASE, e.g. SENSOR");
    namespaceEdit_  = new QLineEdit("NewTarget", grp);
    namespaceEdit_->setPlaceholderText("CamelCase, e.g. Sensor");

    previewLabel_ = new QLabel("", grp);
    previewLabel_->setObjectName("SubLabel");

    form->addRow("Target name:", targetNameEdit_);
    form->addRow("Namespace:",   namespaceEdit_);
    form->addRow("Created path:", previewLabel_);
    root->addWidget(grp);

    // ── Template ──────────────────────────────────────────────────────────────
    auto* tmplGrp    = new QGroupBox("CMD/TLM Template", this);
    auto* tmplLayout = new QVBoxLayout(tmplGrp);
    tmplGroup_      = new QButtonGroup(this);
    tmplGenericBtn_ = new QRadioButton("Generic  (basic structure)", tmplGrp);
    tmplCcsdsBtn_   = new QRadioButton("CCSDS Satellite  (Primary/Secondary Header)", tmplGrp);
    tmplGenericBtn_->setChecked(true);
    tmplGroup_->addButton(tmplGenericBtn_, 0);
    tmplGroup_->addButton(tmplCcsdsBtn_,   1);
    tmplLayout->addWidget(tmplGenericBtn_);
    tmplLayout->addWidget(tmplCcsdsBtn_);
    root->addWidget(tmplGrp);

    // ── Status + buttons ──────────────────────────────────────────────────────
    statusLabel_ = new QLabel("", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    root->addStretch();

    auto* btnRow  = new QHBoxLayout;
    createBtn_    = new QPushButton("Add Target", this);
    auto* cancelBtn = new QPushButton("Cancel", this);
    createBtn_->setObjectName("PrimaryButton");
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(createBtn_);
    root->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(targetNameEdit_, &QLineEdit::textChanged,
            this, &AddTargetDialog::onTargetNameChanged);
    connect(createBtn_, &QPushButton::clicked, this, &AddTargetDialog::onCreate);
    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);

    connect(&vm_, &ViewModels::InfraViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy();
                createBtn_->setEnabled(!busy);
                createBtn_->setText(busy ? "Adding…" : "Add Target");
            });

    connect(&vm_, &ViewModels::InfraViewModel::targetAdded,
            this, [this](const QString& tname, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, "Target Added",
                        QString("Target '%1' was added.\n\n%2").arg(tname).arg(detail));
                    accept();
                } else {
                    QMessageBox::warning(this, "Add Target Failed", detail);
                }
            });

    onTargetNameChanged(); // set initial preview label

    // Land the keyboard on the first field, with its default text selected so
    // the user can type over it immediately.
    targetNameEdit_->setFocus();
    targetNameEdit_->selectAll();
}

void AddTargetDialog::onTargetNameChanged()
{
    const QString tgt = targetNameEdit_->text().trimmed().toUpper();
    previewLabel_->setText(
        pluginRoot_ + "/targets/" + tgt + "/cmd_tlm/, screens/, procedures/");
}

void AddTargetDialog::onCreate()
{
    const QString tname = targetNameEdit_->text().trimmed().toUpper();
    const QString ns    = namespaceEdit_->text().trimmed();

    if (tname.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Enter a target name.");
        return;
    }
    if (tname.contains('\'') || tname.contains(' ')) {
        QMessageBox::warning(this, "Invalid Input",
            "Target names cannot contain spaces or single quotes.");
        return;
    }

    if (QMessageBox::question(this, "Confirm Add Target",
            QString("Target '%1' will be added at:\n%2/targets/%1\n\nContinue?")
                .arg(tname).arg(pluginRoot_),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    vm_.addTargetToPlugin(pluginRoot_, tname, ns, tmplGroup_->checkedId());
}

} // namespace OpenC3::UI::Dialogs
