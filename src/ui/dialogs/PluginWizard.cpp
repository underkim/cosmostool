#include "PluginWizard.h"
#include "viewmodels/plugin/PluginTemplateEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFont>
#include <QFontDatabase>
#include <QMessageBox>
#include <QFrame>

namespace OpenC3::UI::Dialogs {

// ── Constructor ───────────────────────────────────────────────────────────────

PluginWizard::PluginWizard(ViewModels::InfraViewModel& vm, QWidget* parent)
    : QDialog(parent)
    , vm_(vm)
{
    setWindowTitle(tr("New OpenC3 Plugin Wizard"));
    setMinimumSize(720, 580);
    resize(820, 640);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 16);

    // ── Step indicator ────────────────────────────────────────────────────────
    stepIndicator_ = new QLabel(tr("1 / 3  -  Plugin Basics"), this);
    QFont sf = stepIndicator_->font();
    sf.setPointSize(11); sf.setBold(true);
    stepIndicator_->setFont(sf);
    stepIndicator_->setObjectName("SubLabel");
    stepIndicator_->setContentsMargins(20, 14, 20, 8);
    root->addWidget(stepIndicator_);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("Separator");
    root->addWidget(sep);

    // ── Page stack ────────────────────────────────────────────────────────────
    pages_ = new QStackedWidget(this);
    buildInfoPage();
    buildTargetPage();
    buildPreviewPage();
    root->addWidget(pages_, 1);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel("", this);
    statusLabel_->setObjectName("SubLabel");
    statusLabel_->setContentsMargins(20, 4, 20, 0);
    root->addWidget(statusLabel_);

    // ── Navigation buttons ────────────────────────────────────────────────────
    auto* navRow  = new QHBoxLayout;
    navRow->setContentsMargins(20, 8, 20, 0);
    backBtn_   = new QPushButton(tr("< Back"), this);
    nextBtn_   = new QPushButton(tr("Next >"), this);
    finishBtn_ = new QPushButton(tr("Create"), this);
    finishBtn_->setObjectName("PrimaryButton");
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    navRow->addWidget(backBtn_);
    navRow->addStretch();
    navRow->addWidget(cancelBtn);
    navRow->addWidget(nextBtn_);
    navRow->addWidget(finishBtn_);
    root->addLayout(navRow);

    connect(backBtn_,   &QPushButton::clicked, this, &PluginWizard::onBack);
    connect(nextBtn_,   &QPushButton::clicked, this, &PluginWizard::onNext);
    connect(finishBtn_, &QPushButton::clicked, this, &PluginWizard::onFinish);
    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);

    connect(&vm_, &ViewModels::InfraViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy();
                finishBtn_->setEnabled(!busy);
                finishBtn_->setText(busy ? tr("Creating…") : tr("Create"));
            });

    connect(&vm_, &ViewModels::InfraViewModel::scaffoldComplete,
            this, [this](const QString& rootPath, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, tr("Plugin Created"),
                        tr("The plugin was created at:\n%1\n\n%2").arg(rootPath, detail));
                    accept();
                } else {
                    QMessageBox::warning(this, tr("Create Failed"), detail);
                }
            });

    goToStep(0);

    // Land the keyboard on the plugin name field, with its default text
    // selected so the user notices it and can type over it immediately
    // (matches AddTargetDialog's pattern).
    pluginNameEdit_->setFocus();
    pluginNameEdit_->selectAll();
}

// ── Page builders ─────────────────────────────────────────────────────────────

void PluginWizard::buildInfoPage()
{
    auto* page   = new QWidget(pages_);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 8);

    auto* grp  = new QGroupBox(tr("Plugin Info"), page);
    auto* form = new QFormLayout(grp);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    pluginNameEdit_ = new QLineEdit("my-satellite", grp);
    pluginNameEdit_->setPlaceholderText(tr("lowercase-hyphen, e.g. my-satellite"));
    descriptionEdit_ = new QLineEdit(tr("My satellite target plugin"), grp);
    remoteRootEdit_  = new QLineEdit(vm_.defaultPluginsPath(), grp);
    remoteRootEdit_->setFont(QFont("Consolas", 10));

    gemNameLabel_ = new QLabel("<b>cosmos-my-satellite</b>", grp);
    gemNameLabel_->setObjectName("SubLabel");

    form->addRow(tr("Plugin name:"), pluginNameEdit_);
    form->addRow(tr("Description:"), descriptionEdit_);
    form->addRow(tr("Create in:"),   remoteRootEdit_);
    form->addRow(tr("Gem name:"),    gemNameLabel_);

    connect(pluginNameEdit_, &QLineEdit::textChanged,
            this, &PluginWizard::onPluginNameChanged);

    layout->addWidget(grp);
    layout->addStretch();
    pages_->addWidget(page); // index 0
}

void PluginWizard::buildTargetPage()
{
    auto* page   = new QWidget(pages_);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 8);
    layout->setSpacing(12);

    // ── Target info ───────────────────────────────────────────────────────────
    auto* tgtGrp  = new QGroupBox(tr("Target Info"), page);
    auto* tgtForm = new QFormLayout(tgtGrp);
    tgtForm->setLabelAlignment(Qt::AlignRight);
    tgtForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    targetNameEdit_ = new QLineEdit("MYSAT", tgtGrp);
    targetNameEdit_->setPlaceholderText(tr("UPPERCASE, e.g. MYSAT"));
    namespaceEdit_  = new QLineEdit("MySat", tgtGrp);
    namespaceEdit_->setPlaceholderText(tr("CamelCase, e.g. MySat"));

    targetPreviewLabel_ = new QLabel("", tgtGrp);
    targetPreviewLabel_->setObjectName("SubLabel");

    tgtForm->addRow(tr("Target name:"), targetNameEdit_);
    tgtForm->addRow(tr("Namespace:"),   namespaceEdit_);
    tgtForm->addRow(tr("Path preview:"), targetPreviewLabel_);

    connect(targetNameEdit_, &QLineEdit::textChanged,
            this, &PluginWizard::onTargetNameChanged);

    // ── Interface ─────────────────────────────────────────────────────────────
    auto* ifGrp  = new QGroupBox(tr("Interface"), page);
    auto* ifForm = new QFormLayout(ifGrp);
    ifForm->setLabelAlignment(Qt::AlignRight);
    ifForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    ifaceTypeCombo_ = new QComboBox(ifGrp);
    ifaceTypeCombo_->addItems({tr("TCP/IP Client"), tr("TCP/IP Server"), tr("UDP"), tr("Serial")});
    ifaceHostEdit_ = new QLineEdit("localhost", ifGrp);
    ifacePortEdit_ = new QLineEdit("8080", ifGrp);
    ifacePortEdit_->setFixedWidth(80);

    auto* hostRow = new QHBoxLayout;
    hostRow->addWidget(ifaceHostEdit_);
    hostRow->addWidget(new QLabel(tr("Port:"), ifGrp));
    hostRow->addWidget(ifacePortEdit_);

    ifForm->addRow(tr("Type:"),       ifaceTypeCombo_);
    ifForm->addRow(tr("Host:Port:"),  hostRow);

    connect(ifaceTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        // Serial reuses the same two fields for device path + baud rate
        // rather than a network host/port pair - clarify via placeholders.
        if (idx == 3) {
            ifaceHostEdit_->setPlaceholderText(tr("/dev/ttyUSB0"));
            ifacePortEdit_->setPlaceholderText(tr("baud"));
        } else {
            ifaceHostEdit_->setPlaceholderText(tr("localhost"));
            ifacePortEdit_->setPlaceholderText(tr("port"));
        }
    });

    // ── Template ──────────────────────────────────────────────────────────────
    auto* tmplGrp    = new QGroupBox(tr("CMD/TLM Template"), page);
    auto* tmplLayout = new QVBoxLayout(tmplGrp);
    tmplGroup_     = new QButtonGroup(this);
    tmplGenericBtn_ = new QRadioButton(tr("Generic  (basic structure)"), tmplGrp);
    tmplCcsdsBtn_   = new QRadioButton(tr("CCSDS Satellite  (Primary/Secondary Header)"), tmplGrp);
    tmplGseBtn_     = new QRadioButton(tr("GSE Interface  (TCP/IP ground support equipment)"), tmplGrp);
    tmplGenericBtn_->setChecked(true);
    tmplGroup_->addButton(tmplGenericBtn_, 0);
    tmplGroup_->addButton(tmplCcsdsBtn_,   1);
    tmplGroup_->addButton(tmplGseBtn_,     2);
    tmplLayout->addWidget(tmplGenericBtn_);
    tmplLayout->addWidget(tmplCcsdsBtn_);
    tmplLayout->addWidget(tmplGseBtn_);

    layout->addWidget(tgtGrp);
    layout->addWidget(ifGrp);
    layout->addWidget(tmplGrp);
    layout->addStretch();
    pages_->addWidget(page); // index 1

    onTargetNameChanged(); // set initial preview label
}

void PluginWizard::buildPreviewPage()
{
    auto* page   = new QWidget(pages_);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 12, 20, 8);

    auto* hint = new QLabel(tr(
        "Preview and edit the files that will be created. "
        "Each tab's content is written directly to the remote."), page);
    hint->setObjectName("SubLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    previewTabs_ = new QTabWidget(page);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    previewTabs_->setFont(mono);
    layout->addWidget(previewTabs_, 1);

    pages_->addWidget(page); // index 2
}

// ── Navigation ────────────────────────────────────────────────────────────────

void PluginWizard::goToStep(int step)
{
    currentStep_ = step;
    pages_->setCurrentIndex(step);
    updateNavButtons();

    const QStringList headers = {
        tr("1 / 3  -  Plugin Basics"),
        tr("2 / 3  -  Target & Interface"),
        tr("3 / 3  -  Preview & Edit Files")
    };
    if (step < headers.size())
        stepIndicator_->setText(headers[step]);

    if (step == 2)
        refreshPreview();
}

void PluginWizard::updateNavButtons()
{
    backBtn_->setVisible(currentStep_ > 0);
    nextBtn_->setVisible(currentStep_ < 2);
    finishBtn_->setVisible(currentStep_ == 2);
}

void PluginWizard::onBack()
{
    if (currentStep_ > 0) goToStep(currentStep_ - 1);
}

void PluginWizard::onNext()
{
    // Validate current step before advancing
    if (currentStep_ == 0) {
        const QString pname = pluginNameEdit_->text().trimmed();
        if (pname.isEmpty() || pname.contains(' ') || pname.contains('\'')) {
            QMessageBox::warning(this, tr("Invalid Input"),
                tr("Enter the plugin name in lowercase-hyphen form, without spaces or single quotes."));
            return;
        }
    } else if (currentStep_ == 1) {
        if (targetNameEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Input"), tr("Enter a target name."));
            return;
        }
    }
    if (currentStep_ < 2) goToStep(currentStep_ + 1);
}

void PluginWizard::onFinish()
{
    if (QMessageBox::question(this, tr("Confirm Create"),
            tr("The plugin will be created at:\n\n%1/%2\n\nContinue?")
                .arg(remoteRootEdit_->text().trimmed()).arg(gemName()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Write exactly what's in the preview tabs, not a fresh re-generation from
    // the form fields - the Preview step tells the user their edits are what
    // gets written, so it must actually be what gets written.
    QMap<QString, QString> files;
    for (auto it = fileEditors_.cbegin(); it != fileEditors_.cend(); ++it)
        files[it.key()] = it.value()->toPlainText();

    vm_.scaffoldPluginFiles(
        remoteRootEdit_->text().trimmed(),
        pluginNameEdit_->text().trimmed(),
        files);
}

// ── Live update slots ─────────────────────────────────────────────────────────

void PluginWizard::onPluginNameChanged()
{
    gemNameLabel_->setText(tr("<b>cosmos-%1</b>").arg(pluginNameEdit_->text().trimmed()));
}

void PluginWizard::onTargetNameChanged()
{
    const QString tgt = targetNameEdit_->text().trimmed().toUpper();
    const QString plug = pluginNameEdit_->text().trimmed();
    targetPreviewLabel_->setText(
        tr("targets/%1/cmd_tlm/%2_cmds.txt …").arg(tgt).arg(tgt.toLower()));
    Q_UNUSED(plug);
}

void PluginWizard::refreshPreview()
{
    const QString pname = pluginNameEdit_->text().trimmed();
    const QString tname = targetNameEdit_->text().trimmed().toUpper();
    const QString desc  = descriptionEdit_->text().trimmed();

    if (pname.isEmpty() || tname.isEmpty()) return;

    const QMap<QString, QString> files =
        ViewModels::PluginTemplateEngine::buildFiles(
            pname, tname, desc, templateType(),
            ifaceTypeCombo_->currentIndex(),
            ifaceHostEdit_->text().trimmed(),
            ifacePortEdit_->text().trimmed(),
            namespaceEdit_->text().trimmed());

    // Remove and destroy old tab widgets
    while (previewTabs_->count() > 0) {
        QWidget* w = previewTabs_->widget(0);
        previewTabs_->removeTab(0);
        delete w;
    }
    fileEditors_.clear();

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // Show tabs in a logical order
    const QStringList order = {
        "plugin.txt",
        "cosmos-" + pname + ".gemspec",
        "Gemfile",
        "targets/" + tname + "/cmd_tlm/" + tname.toLower() + "_cmds.txt",
        "targets/" + tname + "/cmd_tlm/" + tname.toLower() + "_tlm.txt",
        "targets/" + tname + "/screens/" + tname.toLower() + ".txt",
        "targets/" + tname + "/procedures/" + tname.toLower() + "_check.rb"
    };

    for (const QString& path : order) {
        if (!files.contains(path)) continue;
        auto* ed = new QPlainTextEdit(previewTabs_);
        ed->setFont(mono);
        ed->setPlainText(files[path]);
        const QString tabLabel = path.contains('/')
            ? path.section('/', -1)  // last segment
            : path;
        previewTabs_->addTab(ed, tabLabel);
        fileEditors_[path] = ed;
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString PluginWizard::gemName() const
{
    return "cosmos-" + pluginNameEdit_->text().trimmed();
}

QString PluginWizard::targetUpper() const
{
    return targetNameEdit_->text().trimmed().toUpper();
}

int PluginWizard::templateType() const
{
    return tmplGroup_->checkedId(); // 0=Generic, 1=CCSDS, 2=GSE
}

} // namespace OpenC3::UI::Dialogs
