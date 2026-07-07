#include "NewScriptDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpression>

namespace OpenC3::UI::Dialogs {

namespace {
// The script name becomes a remote file-path segment (procedures/<name>.rb)
// and a Ruby method name (def <name>) - mirrors AddTargetDialog's
// isValidTargetName() path-escape rationale.
bool isValidScriptName(const QString& name)
{
    static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return pattern.match(name).hasMatch();
}

// Target is also a remote file-path segment (targets/<name>/procedures/...)
// - it's editable (the combo can hold typed-in text, not just a known
// target from the list), so it needs the same guard AddTargetDialog
// applies to its own target-name field; without it, a value like
// "../../etc" would write outside the plugin's own target folder instead
// of failing loudly here (mkdir -p/writeFile shell-quote the path string,
// but that doesn't stop ".." from escaping the intended directory).
bool isValidTargetToken(const QString& name)
{
    static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return pattern.match(name).hasMatch();
}
} // namespace

NewScriptDialog::NewScriptDialog(
    ViewModels::InfraViewModel& vm,
    const QString&              pluginRoot,
    const QStringList&          knownTargets,
    QWidget*                    parent)
    : QDialog(parent)
    , vm_(vm)
    , pluginRoot_(pluginRoot)
{
    setWindowTitle(tr("Add Script"));
    setMinimumSize(480, 320);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto* title = new QLabel(
        tr("Add a script (procedure) to a target in this plugin:\n<small>%1</small>")
            .arg(pluginRoot), this);
    title->setTextFormat(Qt::RichText);
    title->setWordWrap(true);
    root->addWidget(title);

    // ── Form ──────────────────────────────────────────────────────────────────
    auto* grp  = new QGroupBox(tr("Script Info"), this);
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    targetCombo_ = new QComboBox(grp);
    targetCombo_->setEditable(true);
    targetCombo_->addItems(knownTargets);
    if (!knownTargets.isEmpty())
        targetCombo_->setCurrentIndex(0);
    else
        targetCombo_->setPlaceholderText(tr("Target name, e.g. MYSAT"));

    scriptNameEdit_ = new QLineEdit("my_check", grp);
    scriptNameEdit_->setObjectName("ScriptNameEdit");
    scriptNameEdit_->setPlaceholderText(tr("lowercase, e.g. my_check"));

    previewLabel_ = new QLabel("", grp);
    previewLabel_->setObjectName("SubLabel");

    form->addRow(tr("Target:"), targetCombo_);
    form->addRow(tr("Script name:"), scriptNameEdit_);
    form->addRow(tr("Created path:"), previewLabel_);
    root->addWidget(grp);

    auto* hint = new QLabel(tr(
        "Generates a starter script with a cmd()/wait_check() example you can "
        "edit and run against a live target - this is the same pattern used "
        "when a new target is first created."), this);
    hint->setWordWrap(true);
    hint->setObjectName("SubLabel");
    root->addWidget(hint);

    // ── Status + buttons ──────────────────────────────────────────────────────
    statusLabel_ = new QLabel("", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    root->addStretch();

    auto* btnRow    = new QHBoxLayout;
    createBtn_      = new QPushButton(tr("Add Script"), this);
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    createBtn_->setObjectName("PrimaryButton");
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(createBtn_);
    root->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(targetCombo_, &QComboBox::currentTextChanged, this, &NewScriptDialog::updatePreview);
    connect(scriptNameEdit_, &QLineEdit::textChanged, this, &NewScriptDialog::updatePreview);
    connect(createBtn_, &QPushButton::clicked, this, &NewScriptDialog::onCreate);
    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);

    connect(&vm_, &ViewModels::InfraViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy();
                createBtn_->setEnabled(!busy);
                createBtn_->setText(busy ? tr("Adding…") : tr("Add Script"));
            });

    connect(&vm_, &ViewModels::InfraViewModel::scriptAdded,
            this, [this](const QString& sname, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, tr("Script Added"),
                        tr("Script '%1' was added.\n\n%2").arg(sname).arg(detail));
                    accept();
                } else {
                    QMessageBox::warning(this, tr("Add Script Failed"), detail);
                }
            });

    updatePreview();

    scriptNameEdit_->setFocus();
    scriptNameEdit_->selectAll();
}

void NewScriptDialog::updatePreview()
{
    const QString tgt  = targetCombo_->currentText().trimmed().toUpper();
    const QString name = scriptNameEdit_->text().trimmed().toLower();
    previewLabel_->setText(
        pluginRoot_ + "/targets/" + tgt + "/procedures/" + name + ".rb");
}

void NewScriptDialog::onCreate()
{
    const QString tgt  = targetCombo_->currentText().trimmed().toUpper();
    const QString name = scriptNameEdit_->text().trimmed().toLower();

    if (tgt.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Choose or enter a target name."));
        return;
    }
    if (!isValidTargetToken(tgt)) {
        QMessageBox::warning(this, tr("Invalid Input"),
            tr("Target names may only contain letters, numbers, and underscores, "
               "and must start with a letter or underscore."));
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Enter a script name."));
        return;
    }
    if (!isValidScriptName(name)) {
        QMessageBox::warning(this, tr("Invalid Input"),
            tr("Script names may only contain letters, numbers, and underscores, "
               "and must start with a letter or underscore."));
        return;
    }

    if (QMessageBox::question(this, tr("Confirm Add Script"),
            tr("Script '%1' will be added at:\n%2/targets/%3/procedures/%1.rb\n\nContinue?")
                .arg(name).arg(pluginRoot_).arg(tgt),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    vm_.addScriptToPlugin(pluginRoot_, tgt, name);
}

} // namespace OpenC3::UI::Dialogs
