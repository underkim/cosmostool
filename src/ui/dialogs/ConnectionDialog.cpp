#include "ConnectionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>

namespace OpenC3::UI::Dialogs {

ConnectionDialog::ConnectionDialog(
    ViewModels::SettingsViewModel& vm, QWidget* parent)
    : QDialog(parent)
    , vm_(vm)
{
    setWindowTitle("Connect to OpenC3 Environment");
    setFixedSize(420, 180);
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
                if (state == "Connected") accept();
            });

    vm_.loadProfiles();
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
