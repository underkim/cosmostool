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
    setWindowTitle("New OpenC3 Plugin Wizard");
    setMinimumSize(720, 580);
    resize(820, 640);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 16);

    // ── Step indicator ────────────────────────────────────────────────────────
    stepIndicator_ = new QLabel("1 / 3  -  Plugin Basics", this);
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
    backBtn_   = new QPushButton("< Back", this);
    nextBtn_   = new QPushButton("Next >", this);
    finishBtn_ = new QPushButton("Create", this);
    finishBtn_->setObjectName("PrimaryButton");
    auto* cancelBtn = new QPushButton("Cancel", this);
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
                finishBtn_->setText(busy ? "Creating…" : "Create");
            });

    connect(&vm_, &ViewModels::InfraViewModel::scaffoldComplete,
            this, [this](const QString& rootPath, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, "Plugin Created",
                        "The plugin was created at:\n" + rootPath + "\n\n" + detail);
                    accept();
                } else {
                    QMessageBox::warning(this, "Create Failed", detail);
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

    auto* grp  = new QGroupBox("Plugin Info", page);
    auto* form = new QFormLayout(grp);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    pluginNameEdit_ = new QLineEdit("my-satellite", grp);
    pluginNameEdit_->setPlaceholderText("lowercase-hyphen, e.g. my-satellite");
    descriptionEdit_ = new QLineEdit("My satellite target plugin", grp);
    remoteRootEdit_  = new QLineEdit(vm_.defaultPluginsPath(), grp);
    remoteRootEdit_->setFont(QFont("Consolas", 10));

    gemNameLabel_ = new QLabel("<b>cosmos-my-satellite</b>", grp);
    gemNameLabel_->setObjectName("SubLabel");

    form->addRow("Plugin name:", pluginNameEdit_);
    form->addRow("Description:", descriptionEdit_);
    form->addRow("Create in:",   remoteRootEdit_);
    form->addRow("Gem name:",    gemNameLabel_);

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
    auto* tgtGrp  = new QGroupBox("Target Info", page);
    auto* tgtForm = new QFormLayout(tgtGrp);
    tgtForm->setLabelAlignment(Qt::AlignRight);
    tgtForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    targetNameEdit_ = new QLineEdit("MYSAT", tgtGrp);
    targetNameEdit_->setPlaceholderText("UPPERCASE, e.g. MYSAT");
    namespaceEdit_  = new QLineEdit("MySat", tgtGrp);
    namespaceEdit_->setPlaceholderText("CamelCase, e.g. MySat");

    targetPreviewLabel_ = new QLabel("", tgtGrp);
    targetPreviewLabel_->setObjectName("SubLabel");

    tgtForm->addRow("Target name:", targetNameEdit_);
    tgtForm->addRow("Namespace:",   namespaceEdit_);
    tgtForm->addRow("Path preview:", targetPreviewLabel_);

    connect(targetNameEdit_, &QLineEdit::textChanged,
            this, &PluginWizard::onTargetNameChanged);

    // ── Interface ─────────────────────────────────────────────────────────────
    auto* ifGrp  = new QGroupBox("Interface", page);
    auto* ifForm = new QFormLayout(ifGrp);
    ifForm->setLabelAlignment(Qt::AlignRight);
    ifForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    ifaceTypeCombo_ = new QComboBox(ifGrp);
    ifaceTypeCombo_->addItems({"TCP/IP Client", "TCP/IP Server", "UDP", "Serial"});
    ifaceHostEdit_ = new QLineEdit("localhost", ifGrp);
    ifacePortEdit_ = new QLineEdit("8080", ifGrp);
    ifacePortEdit_->setFixedWidth(80);

    auto* hostRow = new QHBoxLayout;
    hostRow->addWidget(ifaceHostEdit_);
    hostRow->addWidget(new QLabel("Port:", ifGrp));
    hostRow->addWidget(ifacePortEdit_);

    ifForm->addRow("Type:",       ifaceTypeCombo_);
    ifForm->addRow("Host:Port:",  hostRow);

    // ── Template ──────────────────────────────────────────────────────────────
    auto* tmplGrp    = new QGroupBox("CMD/TLM Template", page);
    auto* tmplLayout = new QVBoxLayout(tmplGrp);
    tmplGroup_     = new QButtonGroup(this);
    tmplGenericBtn_ = new QRadioButton("Generic  (basic structure)", tmplGrp);
    tmplCcsdsBtn_   = new QRadioButton("CCSDS Satellite  (Primary/Secondary Header)", tmplGrp);
    tmplGseBtn_     = new QRadioButton("GSE Interface  (TCP/IP ground support equipment)", tmplGrp);
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

    auto* hint = new QLabel(
        "Preview and edit the files that will be created. "
        "Each tab's content is written directly to the remote.", page);
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
        "1 / 3  -  Plugin Basics",
        "2 / 3  -  Target & Interface",
        "3 / 3  -  Preview & Edit Files"
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
            QMessageBox::warning(this, "Invalid Input",
                "Enter the plugin name in lowercase-hyphen form, without spaces or single quotes.");
            return;
        }
    } else if (currentStep_ == 1) {
        if (targetNameEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Enter a target name.");
            return;
        }
    }
    if (currentStep_ < 2) goToStep(currentStep_ + 1);
}

void PluginWizard::onFinish()
{
    if (QMessageBox::question(this, "Confirm Create",
            QString("The plugin will be created at:\n\n%1/%2\n\nContinue?")
                .arg(remoteRootEdit_->text().trimmed()).arg(gemName()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Collect (possibly edited) file contents from preview tabs
    // and update the file map, then call scaffoldPlugin.
    // For now we call scaffoldPlugin with wizard params; edited content
    // would require a custom upload path (future enhancement).
    vm_.scaffoldPlugin(
        remoteRootEdit_->text().trimmed(),
        pluginNameEdit_->text().trimmed(),
        targetNameEdit_->text().trimmed().toUpper(),
        namespaceEdit_->text().trimmed(),
        descriptionEdit_->text().trimmed(),
        templateType());
}

// ── Live update slots ─────────────────────────────────────────────────────────

void PluginWizard::onPluginNameChanged()
{
    gemNameLabel_->setText("<b>cosmos-" + pluginNameEdit_->text().trimmed() + "</b>");
}

void PluginWizard::onTargetNameChanged()
{
    const QString tgt = targetNameEdit_->text().trimmed().toUpper();
    const QString plug = pluginNameEdit_->text().trimmed();
    targetPreviewLabel_->setText(
        QString("targets/%1/cmd_tlm/%2_cmds.txt …").arg(tgt).arg(tgt.toLower()));
    Q_UNUSED(plug);
}

void PluginWizard::refreshPreview()
{
    const QString pname = pluginNameEdit_->text().trimmed();
    const QString tname = targetNameEdit_->text().trimmed().toUpper();
    const QString desc  = descriptionEdit_->text().trimmed();

    if (pname.isEmpty() || tname.isEmpty()) return;

    const QMap<QString, QString> files =
        ViewModels::PluginTemplateEngine::buildFiles(pname, tname, desc, templateType());

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
