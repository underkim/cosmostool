#include "PluginScaffoldDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QFont>
#include <QMessageBox>

namespace OpenC3::UI::Dialogs {

PluginScaffoldDialog::PluginScaffoldDialog(
    ViewModels::InfraViewModel& vm, QWidget* parent)
    : QDialog(parent)
    , vm_(vm)
{
    setWindowTitle("새 OpenC3 플러그인 스캐폴딩");
    setMinimumSize(780, 580);
    resize(860, 620);
    setupUi();
    bindViewModel();
    updatePreview();
}

void PluginScaffoldDialog::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* title = new QLabel("🔨 플러그인 스캐폴딩", this);
    QFont tf = title->font(); tf.setPointSize(15); tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left: form ────────────────────────────────────────────────────────────
    auto* formGroup  = new QGroupBox("플러그인 정보", splitter);
    auto* formLayout = new QFormLayout(formGroup);
    formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    pluginNameEdit_ = new QLineEdit("my-satellite", formGroup);
    pluginNameEdit_->setPlaceholderText("예: my-satellite  (소문자 하이픈)");
    targetNameEdit_ = new QLineEdit("MYSAT", formGroup);
    targetNameEdit_->setPlaceholderText("예: MYSAT  (대문자)");
    namespaceEdit_  = new QLineEdit("MySat", formGroup);
    namespaceEdit_->setPlaceholderText("예: MySat");
    descriptionEdit_ = new QLineEdit("My satellite target", formGroup);
    remoteRootEdit_  = new QLineEdit("/cosmos/plugins", formGroup);
    remoteRootEdit_->setPlaceholderText("플러그인을 생성할 원격 경로");

    formLayout->addRow("플러그인 이름:", pluginNameEdit_);
    formLayout->addRow("타겟 이름:",    targetNameEdit_);
    formLayout->addRow("네임스페이스:", namespaceEdit_);
    formLayout->addRow("설명:",         descriptionEdit_);
    formLayout->addRow("생성 위치:",    remoteRootEdit_);

    // ── Template selector ─────────────────────────────────────────────────────
    auto* tmplGroup = new QGroupBox("템플릿 유형", formGroup);
    auto* tmplLayout = new QVBoxLayout(tmplGroup);
    templateGroup_   = new QButtonGroup(this);

    auto addRadio = [&](const QString& label, const QString& hint) {
        auto* btn = new QRadioButton(label, tmplGroup);
        auto* lbl = new QLabel("<small style='color:#858585'>" + hint + "</small>",
                               tmplGroup);
        lbl->setTextFormat(Qt::RichText);
        auto* row = new QHBoxLayout;
        row->addWidget(btn);
        row->addWidget(lbl);
        row->addStretch();
        tmplLayout->addLayout(row);
        templateGroup_->addButton(btn, templateGroup_->buttons().size());
        return btn;
    };

    tmplGenericBtn_   = addRadio("Generic Target",
        "기본 구조 — 직접 커스텀");
    tmplSatelliteBtn_ = addRadio("Satellite Target (CCSDS)",
        "CCSDS 헤더 포함 CMD/TLM 정의");
    tmplInterfaceBtn_ = addRadio("GSE Interface",
        "TCP/IP 기반 지상 지원 장비 인터페이스");
    tmplToolBtn_      = addRadio("COSMOS Tool (Vue.js)",
        "웹 기반 도구 플러그인 스텁");

    tmplGenericBtn_->setChecked(true);
    formLayout->addRow(tmplGroup);

    // ── Right: preview ────────────────────────────────────────────────────────
    auto* previewGroup  = new QGroupBox("생성될 파일 구조 미리보기", splitter);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    previewEdit_ = new QTextEdit(previewGroup);
    QFont f; f.setFamily("Consolas"); f.setPointSize(10);
    previewEdit_->setFont(f);
    previewEdit_->setReadOnly(true);
    previewLayout->addWidget(previewEdit_);

    splitter->addWidget(formGroup);
    splitter->addWidget(previewGroup);
    splitter->setSizes({360, 460});
    root->addWidget(splitter, 1);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel("", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* btnBar  = new QHBoxLayout;
    previewBtn_   = new QPushButton("🔍 미리보기 갱신", this);
    createBtn_    = new QPushButton("🚀 원격에 생성", this);
    cancelBtn_    = new QPushButton("취소", this);
    createBtn_->setObjectName("PrimaryButton");
    btnBar->addWidget(previewBtn_);
    btnBar->addStretch();
    btnBar->addWidget(createBtn_);
    btnBar->addWidget(cancelBtn_);
    root->addLayout(btnBar);
}

void PluginScaffoldDialog::bindViewModel()
{
    connect(previewBtn_, &QPushButton::clicked, this, &PluginScaffoldDialog::updatePreview);
    connect(createBtn_,  &QPushButton::clicked, this, &PluginScaffoldDialog::onCreate);
    connect(cancelBtn_,  &QPushButton::clicked, this, &QDialog::reject);

    // Live preview on text change
    for (auto* edit : {pluginNameEdit_, targetNameEdit_,
                        namespaceEdit_,  descriptionEdit_}) {
        connect(edit, &QLineEdit::textChanged,
                this, &PluginScaffoldDialog::updatePreview);
    }
    connect(templateGroup_, &QButtonGroup::idClicked,
            this, [this](int) { updatePreview(); });

    connect(&vm_, &ViewModels::InfraViewModel::busyChanged,
            this, [this] {
                const bool busy = vm_.isBusy();
                createBtn_->setEnabled(!busy);
                previewBtn_->setEnabled(!busy);
                createBtn_->setText(busy ? "생성 중…" : "🚀 원격에 생성");
            });

    connect(&vm_, &ViewModels::InfraViewModel::scaffoldComplete,
            this, [this](const QString& rootPath, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, "스캐폴딩 완료",
                        "플러그인 구조가 생성되었습니다:\n" + rootPath + "\n\n" + detail);
                    accept();
                } else {
                    QMessageBox::warning(this, "생성 실패", detail);
                }
            });
}

void PluginScaffoldDialog::updatePreview()
{
    previewEdit_->setPlainText(buildPreviewTree());
}

QString PluginScaffoldDialog::buildPreviewTree() const
{
    const QString pname = pluginNameEdit_->text().trimmed().isEmpty()
                          ? "my-plugin" : pluginNameEdit_->text().trimmed();
    const QString tname = targetNameEdit_->text().trimmed().toUpper().isEmpty()
                          ? "MYTARGET" : targetNameEdit_->text().trimmed().toUpper();
    const QString gem   = "cosmos-" + pname;
    const QString tl    = tname.toLower();
    const bool isSat    = tmplSatelliteBtn_->isChecked();
    const bool isTool   = tmplToolBtn_->isChecked();

    QString tree;
    tree += gem + "/\n";
    tree += "├─ plugin.txt\n";
    tree += "├─ " + gem + ".gemspec\n";
    tree += "├─ Gemfile\n";
    tree += "└─ targets/\n";
    tree += "   └─ " + tname + "/\n";
    tree += "      ├─ cmd_tlm/\n";
    if (isSat) {
        tree += "      │  ├─ " + tl + "_cmds.txt     ← CCSDS commands\n";
        tree += "      │  └─ " + tl + "_tlm.txt      ← CCSDS telemetry\n";
    } else {
        tree += "      │  ├─ " + tl + "_cmds.txt\n";
        tree += "      │  └─ " + tl + "_tlm.txt\n";
    }
    tree += "      ├─ screens/\n";
    tree += "      │  └─ " + tl + ".txt             ← TLM Viewer 화면\n";
    tree += "      └─ procedures/\n";
    tree += "         └─ " + tl + "_check.rb        ← 체크아웃 스크립트\n";

    if (isTool) {
        tree += "\n(Tool 템플릿은 추가 파일 포함)\n";
        tree += gem + "/\n";
        tree += "└─ tools/\n";
        tree += "   └─ " + pname + "/\n";
        tree += "      └─ src/                      ← Vue.js 소스\n";
    }

    tree += "\n────────────────────────────────────\n";
    tree += "생성 위치: " + remoteRootEdit_->text().trimmed() +
            "/" + gem + "/\n";
    tree += "────────────────────────────────────\n";

    // Show what will be in plugin.txt
    tree += "\n[ plugin.txt 미리보기 ]\n";
    const QString varPfx = pname.toLower().replace('-', '_');
    tree += "VARIABLE " + varPfx + "_target_name " + tname + "\n";
    tree += "TARGET " + tname + " <%= " + varPfx + "_target_name %>\n";
    tree += "INTERFACE <%= " + varPfx + "_target_name %>_INT \\\n";
    tree += "  tcpip_client_interface.rb localhost 8080 8080 10 nil BURST\n";
    tree += "  MAP_TARGET <%= " + varPfx + "_target_name %>\n";

    return tree;
}

void PluginScaffoldDialog::onCreate()
{
    const QString pname = pluginNameEdit_->text().trimmed();
    const QString tname = targetNameEdit_->text().trimmed();

    if (pname.isEmpty() || tname.isEmpty()) {
        QMessageBox::warning(this, "입력 필요",
            "플러그인 이름과 타겟 이름을 입력해주세요.");
        return;
    }
    if (pname.contains(' ') || pname.contains('/')) {
        QMessageBox::warning(this, "이름 오류",
            "플러그인 이름에 공백이나 슬래시를 사용할 수 없습니다.");
        return;
    }

    if (QMessageBox::question(this, "원격 생성 확인",
            "다음 경로에 플러그인 구조를 생성합니다:\n\n" +
            remoteRootEdit_->text().trimmed() + "/cosmos-" + pname + "/\n\n"
            "계속하시겠습니까?",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    int tmplType = 0;
    if (tmplSatelliteBtn_->isChecked())  tmplType = 1;
    else if (tmplInterfaceBtn_->isChecked()) tmplType = 2;
    else if (tmplToolBtn_->isChecked())  tmplType = 3;

    vm_.scaffoldPlugin(
        remoteRootEdit_->text().trimmed(),
        pname,
        tname.toUpper(),
        namespaceEdit_->text().trimmed(),
        descriptionEdit_->text().trimmed(),
        tmplType);
}

} // namespace OpenC3::UI::Dialogs
