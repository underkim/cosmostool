#include "ConnectionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTimer>

namespace OpenC3::UI::Dialogs {

ConnectionDialog::ConnectionDialog(
    ViewModels::SettingsViewModel& vm, QWidget* parent)
    : QDialog(parent)
    , vm_(vm)
{
    setWindowTitle("Connect to OpenC3 Environment");
    // Minimum (not fixed) so long/translated status text can grow the dialog
    // instead of being clipped.
    setMinimumSize(420, 180);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    layout->addWidget(new QLabel("Select connection profile:", this));

    profileCombo_ = new QComboBox(this);
    profileCombo_->setModel(vm_.profileModel());
    layout->addWidget(profileCombo_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    auto* btnRow  = new QHBoxLayout;
    connectBtn_   = new QPushButton("Connect", this);
    connectBtn_->setObjectName("PrimaryButton");
    auto* skipBtn = new QPushButton("Skip", this);
    connect(connectBtn_, &QPushButton::clicked, this, &ConnectionDialog::onConnectClicked);
    connect(skipBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addStretch();
    btnRow->addWidget(skipBtn);
    btnRow->addWidget(connectBtn_);
    layout->addLayout(btnRow);

    connect(&vm_, &ViewModels::SettingsViewModel::connectionStateChanged,
            this, [this](const QString& state) {
                statusLabel_->setText(state);
                const bool connecting = (state == "Connecting");
                const bool hasProfiles = (profileCombo_->count() > 0);
                connectBtn_->setEnabled(hasProfiles && !connecting);
                if (state == "Connected") accept();
            });

    vm_.loadProfiles();

    // Pre-select the default profile (if any), then auto-connect once the
    // event loop is running so the dialog is fully visible before we start.
    const int n = profileCombo_->count();
    for (int i = 0; i < n; ++i) {
        const auto* p = vm_.profileModel()->profileAt(i);
        if (p && p->isDefault) {
            profileCombo_->setCurrentIndex(i);
            // Defer the actual connect until exec() has started its event loop.
            QTimer::singleShot(0, this, &ConnectionDialog::onConnectClicked);
            break;
        }
    }

    if (n == 0) {
        statusLabel_->setText("저장된 프로필이 없습니다. 설정에서 프로필을 추가하세요.");
        connectBtn_->setEnabled(false);
    }
}

void ConnectionDialog::onConnectClicked()
{
    const auto idx = profileCombo_->currentIndex();
    if (idx < 0) return;
    const auto* p = vm_.profileModel()->profileAt(idx);
    if (!p) return;
    connectBtn_->setEnabled(false);
    statusLabel_->setText("Connecting…");
    vm_.connectToProfile(QString::fromStdString(p->id));
}

} // namespace OpenC3::UI::Dialogs
