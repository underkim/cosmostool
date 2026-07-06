#include "NewMicroserviceDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace OpenC3::UI::Dialogs {

namespace {
bool isValidBlockName(const QString& name)
{
    static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
    return pattern.match(name).hasMatch();
}
} // namespace

NewMicroserviceDialog::NewMicroserviceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Microservice"));
    setModal(true);
    resize(420, 220);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    folderEdit_ = new QLineEdit("NEW_FOLDER", this);
    folderEdit_->setPlaceholderText(tr("UPPERCASE, e.g. MY_MICROSERVICE"));

    nameEdit_ = new QLineEdit("NEW_MICROSERVICE", this);
    nameEdit_->setPlaceholderText(tr("UPPERCASE, e.g. MY_MICROSERVICE"));

    cmdEdit_ = new QLineEdit("ruby new_microservice.rb", this);
    cmdEdit_->setPlaceholderText(tr("ruby <script>.rb"));

    form->addRow(tr("Folder:"), folderEdit_);
    form->addRow(tr("Name:"), nameEdit_);
    form->addRow(tr("CMD:"), cmdEdit_);
    root->addLayout(form);

    auto* hint = new QLabel(tr(
        "A Microservice runs a script you write yourself under this folder - "
        "this only adds its plugin.txt declaration."), this);
    hint->setWordWrap(true);
    hint->setObjectName("SubLabel");
    root->addWidget(hint);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const QString folder = folderEdit_->text().trimmed().toUpper();
        const QString name   = nameEdit_->text().trimmed().toUpper();
        if (folder.isEmpty() || !isValidBlockName(folder)
            || name.isEmpty() || !isValidBlockName(name)) {
            QMessageBox::warning(this, windowTitle(),
                tr("Folder and Name must start with a letter or underscore and "
                   "contain only letters, numbers, and underscores."));
            return;
        }
        if (cmdEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Enter a CMD line."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    folderEdit_->setFocus();
    folderEdit_->selectAll();
}

QString NewMicroserviceDialog::generatedBlock() const
{
    const QString folder = folderEdit_->text().trimmed().toUpper();
    const QString name   = nameEdit_->text().trimmed().toUpper();
    const QString cmd    = cmdEdit_->text().trimmed();

    return QString("\nMICROSERVICE %1 %2\n  CMD %3\n").arg(folder, name, cmd);
}

} // namespace OpenC3::UI::Dialogs
