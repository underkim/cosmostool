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
    setWindowTitle("새 OpenC3 플러그인 생성 위자드");
    setMinimumSize(720, 580);
    resize(820, 640);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 16);

    // ── Step indicator ────────────────────────────────────────────────────────
    stepIndicator_ = new QLabel("1 / 3  —  플러그인 기본 정보", this);
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
    backBtn_   = new QPushButton("< 이전", this);
    nextBtn_   = new QPushButton("다음 >", this);
    finishBtn_ = new QPushButton("🚀 생성", this);
    finishBtn_->setObjectName("PrimaryButton");
    auto* cancelBtn = new QPushButton("취소", this);
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
                finishBtn_->setText(busy ? "생성 중…" : "🚀 생성");
            });

    connect(&vm_, &ViewModels::InfraViewModel::scaffoldComplete,
            this, [this](const QString& rootPath, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, "생성 완료",
                        "플러그인이 생성되었습니다:\n" + rootPath + "\n\n" + detail);
                    accept();
                } else {
                    QMessageBox::warning(this, "생성 실패", detail);
                }
            });

    goToStep(0);
}

// ── Page builders ─────────────────────────────────────────────────────────────

void PluginWizard::buildInfoPage()
{
    auto* page   = new QWidget(pages_);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 8);

    auto* grp  = new QGroupBox("플러그인 정보", page);
    auto* form = new QFormLayout(grp);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    pluginNameEdit_ = new QLineEdit("my-satellite", grp);
    pluginNameEdit_->setPlaceholderText("소문자-하이픈  예: my-satellite");
    descriptionEdit_ = new QLineEdit("My satellite target plugin", grp);
    remoteRootEdit_  = new QLineEdit(vm_.defaultPluginsPath(), grp);
    remoteRootEdit_->setFont(QFont("Consolas", 10));

    gemNameLabel_ = new QLabel("<b>cosmos-my-satellite</b>", grp);
    gemNameLabel_->setObjectName("SubLabel");

    form->addRow("플러그인 이름:", pluginNameEdit_);
    form->addRow("설명:",         descriptionEdit_);
    form->addRow("생성 위치:",    remoteRootEdit_);
    form->addRow("gem 이름:",     gemNameLabel_);

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
    auto* tgtGrp  = new QGroupBox("타겟 정보", page);
    auto* tgtForm = new QFormLayout(tgtGrp);
    tgtForm->setLabelAlignment(Qt::AlignRight);
    tgtForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    targetNameEdit_ = new QLineEdit("MYSAT", tgtGrp);
    targetNameEdit_->setPlaceholderText("대문자  예: MYSAT");
    namespaceEdit_  = new QLineEdit("MySat", tgtGrp);
    namespaceEdit_->setPlaceholderText("CamelCase  예: MySat");

    targetPreviewLabel_ = new QLabel("", tgtGrp);
    targetPreviewLabel_->setObjectName("SubLabel");

    tgtForm->addRow("타겟 이름:",     targetNameEdit_);
    tgtForm->addRow("네임스페이스:", namespaceEdit_);
    tgtForm->addRow("경로 미리보기:", targetPreviewLabel_);

    connect(targetNameEdit_, &QLineEdit::textChanged,
            this, &PluginWizard::onTargetNameChanged);

    // ── Interface ─────────────────────────────────────────────────────────────
    auto* ifGrp  = new QGroupBox("인터페이스", page);
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

    ifForm->addRow("타입:",       ifaceTypeCombo_);
    ifForm->addRow("Host:Port:",  hostRow);

    // ── Template ──────────────────────────────────────────────────────────────
    auto* tmplGrp    = new QGroupBox("CMD/TLM 템플릿", page);
    auto* tmplLayout = new QVBoxLayout(tmplGrp);
    tmplGroup_     = new QButtonGroup(this);
    tmplGenericBtn_ = new QRadioButton("Generic  (기본 구조)", tmplGrp);
    tmplCcsdsBtn_   = new QRadioButton("CCSDS Satellite  (Primary/Secondary Header)", tmplGrp);
    tmplGseBtn_     = new QRadioButton("GSE Interface  (TCP/IP 지상 지원 장비)", tmplGrp);
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
        "생성될 파일을 미리 보고 편집할 수 있습니다. "
        "각 탭의 내용이 원격에 직접 저장됩니다.", page);
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
        "1 / 3  —  플러그인 기본 정보",
        "2 / 3  —  타겟 및 인터페이스 설정",
        "3 / 3  —  파일 미리보기 및 편집"
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
            QMessageBox::warning(this, "입력 오류",
                "플러그인 이름은 공백/작은따옴표 없이 소문자-하이픈 형식으로 입력하세요.");
            return;
        }
    } else if (currentStep_ == 1) {
        if (targetNameEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "입력 오류", "타겟 이름을 입력하세요.");
            return;
        }
    }
    if (currentStep_ < 2) goToStep(currentStep_ + 1);
}

void PluginWizard::onFinish()
{
    if (QMessageBox::question(this, "생성 확인",
            QString("다음 경로에 플러그인을 생성합니다:\n\n%1/%2\n\n계속하시겠습니까?")
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
