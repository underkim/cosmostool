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
    setWindowTitle("타겟 추가");
    setFixedSize(480, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto* title = new QLabel(
        QString("플러그인에 타겟 추가:\n<small>%1</small>").arg(pluginRoot), this);
    title->setTextFormat(Qt::RichText);
    title->setWordWrap(true);
    root->addWidget(title);

    // ── Form ──────────────────────────────────────────────────────────────────
    auto* grp  = new QGroupBox("타겟 정보", this);
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    targetNameEdit_ = new QLineEdit("NEWTARGET", grp);
    targetNameEdit_->setPlaceholderText("대문자  예: SENSOR");
    namespaceEdit_  = new QLineEdit("NewTarget", grp);
    namespaceEdit_->setPlaceholderText("CamelCase  예: Sensor");

    previewLabel_ = new QLabel("", grp);
    previewLabel_->setObjectName("SubLabel");

    form->addRow("타겟 이름:",     targetNameEdit_);
    form->addRow("네임스페이스:", namespaceEdit_);
    form->addRow("생성 경로:",    previewLabel_);
    root->addWidget(grp);

    // ── Template ──────────────────────────────────────────────────────────────
    auto* tmplGrp    = new QGroupBox("CMD/TLM 템플릿", this);
    auto* tmplLayout = new QVBoxLayout(tmplGrp);
    tmplGroup_      = new QButtonGroup(this);
    tmplGenericBtn_ = new QRadioButton("Generic  (기본 구조)", tmplGrp);
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
    createBtn_    = new QPushButton("✅ 타겟 추가", this);
    auto* cancelBtn = new QPushButton("취소", this);
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
                createBtn_->setText(busy ? "추가 중…" : "✅ 타겟 추가");
            });

    connect(&vm_, &ViewModels::InfraViewModel::targetAdded,
            this, [this](const QString& tname, bool ok, const QString& detail) {
                statusLabel_->setText(detail);
                if (ok) {
                    QMessageBox::information(this, "타겟 추가 완료",
                        QString("타겟 '%1'이 추가되었습니다.\n\n%2").arg(tname).arg(detail));
                    accept();
                } else {
                    QMessageBox::warning(this, "타겟 추가 실패", detail);
                }
            });

    onTargetNameChanged(); // set initial preview label
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
        QMessageBox::warning(this, "입력 오류", "타겟 이름을 입력하세요.");
        return;
    }
    if (tname.contains('\'') || tname.contains(' ')) {
        QMessageBox::warning(this, "입력 오류",
            "타겟 이름에 공백이나 작은따옴표를 사용할 수 없습니다.");
        return;
    }

    if (QMessageBox::question(this, "타겟 추가 확인",
            QString("다음 경로에 타겟 '%1'을 추가합니다:\n%2/targets/%1\n\n계속하시겠습니까?")
                .arg(tname).arg(pluginRoot_),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    vm_.addTargetToPlugin(pluginRoot_, tname, ns, tmplGroup_->checkedId());
}

} // namespace OpenC3::UI::Dialogs
