#include "PluginManifestInterfaceDialog.h"

#include "viewmodels/plugin/PluginTemplateEngine.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
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

PluginManifestInterfaceDialog::PluginManifestInterfaceDialog(
    bool isRouter, const QStringList& knownTargets, QWidget* parent)
    : QDialog(parent)
    , isRouter_(isRouter)
{
    setWindowTitle(isRouter_ ? tr("New Router") : tr("New Interface"));
    setModal(true);
    resize(420, 260);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    nameEdit_ = new QLineEdit(isRouter_ ? "NEW_ROUTER" : "NEW_INTERFACE", this);
    nameEdit_->setPlaceholderText(tr("UPPERCASE, e.g. MYSAT_INT"));

    targetCombo_ = new QComboBox(this);
    targetCombo_->setEditable(true);
    targetCombo_->addItems(knownTargets);
    if (knownTargets.isEmpty())
        targetCombo_->setPlaceholderText(tr("Target name (MAP_TARGET)"));

    ifaceTypeCombo_ = new QComboBox(this);
    ifaceTypeCombo_->addItems({tr("TCP/IP Client"), tr("TCP/IP Server"), tr("UDP"), tr("Serial")});

    hostEdit_ = new QLineEdit("localhost", this);
    portEdit_ = new QLineEdit("8080", this);
    portEdit_->setFixedWidth(80);

    auto* hostRow = new QHBoxLayout;
    hostRow->addWidget(hostEdit_);
    hostRow->addWidget(new QLabel(tr("Port:"), this));
    hostRow->addWidget(portEdit_);

    form->addRow(tr("Name:"), nameEdit_);
    form->addRow(tr("Maps to target:"), targetCombo_);
    form->addRow(tr("Connection type:"), ifaceTypeCombo_);
    form->addRow(tr("Host:Port:"), hostRow);
    root->addLayout(form);

    connect(ifaceTypeCombo_, &QComboBox::currentIndexChanged,
            this, &PluginManifestInterfaceDialog::updateHostPortPlaceholders);
    updateHostPortPlaceholders(0);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const QString name = nameEdit_->text().trimmed().toUpper();
        if (name.isEmpty() || !isValidBlockName(name)) {
            QMessageBox::warning(this, windowTitle(),
                tr("Name must start with a letter or underscore and contain only "
                   "letters, numbers, and underscores (no spaces or punctuation)."));
            return;
        }
        if (targetCombo_->currentText().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Select or enter a target to map to."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    nameEdit_->setFocus();
    nameEdit_->selectAll();
}

void PluginManifestInterfaceDialog::updateHostPortPlaceholders(int ifaceTypeIndex)
{
    // Serial reuses the same two fields for device path + baud rate rather
    // than a network host/port pair - matches PluginWizard's own interface
    // step, so the same mental model carries over here.
    if (ifaceTypeIndex == 3) {
        hostEdit_->setPlaceholderText(tr("/dev/ttyUSB0"));
        portEdit_->setPlaceholderText(tr("baud"));
    } else {
        hostEdit_->setPlaceholderText(tr("localhost"));
        portEdit_->setPlaceholderText(tr("port"));
    }
}

QString PluginManifestInterfaceDialog::generatedBlock() const
{
    const QString name = nameEdit_->text().trimmed().toUpper();
    const QString target = targetCombo_->currentText().trimmed().toUpper();
    const QString args = ViewModels::PluginTemplateEngine::buildInterfaceArgs(
        ifaceTypeCombo_->currentIndex(), hostEdit_->text().trimmed(), portEdit_->text().trimmed());

    return QString("\n%1 %2 %3\n  MAP_TARGET %4\n")
        .arg(isRouter_ ? "ROUTER" : "INTERFACE", name, args, target);
}

} // namespace OpenC3::UI::Dialogs
