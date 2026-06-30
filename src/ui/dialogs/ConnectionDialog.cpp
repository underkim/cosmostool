#include "ConnectionDialog.h"

#include "models/ConnectionProfile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTimer>
#include <QUuid>

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

    // One-click WSL setup so a first-time user is never stuck with no profile:
    // creates a default WSL (Ubuntu) profile, saves it, and connects.
    quickWslBtn_ = new QPushButton("Create WSL profile && Connect", this);
    quickWslBtn_->setToolTip(
        "Creates a default WSL profile (Ubuntu, /cosmos) and connects to it.\n"
        "You can fine-tune it later under Settings.");
    connect(quickWslBtn_, &QPushButton::clicked,
            this, &ConnectionDialog::onCreateWslProfile);
    layout->addWidget(quickWslBtn_);

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
                quickWslBtn_->setEnabled(!connecting);
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
        statusLabel_->setText(
            "No saved profiles yet. Click \"Create WSL profile & Connect\" to "
            "get started in one step, or Skip to explore first.");
        connectBtn_->setEnabled(false);
    }
}

void ConnectionDialog::onCreateWslProfile()
{
    // Build a sensible default WSL profile so first-run needs no manual setup.
    Models::ConnectionProfile p;
    p.id              = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    p.name            = "WSL (Ubuntu)";
    p.mode            = Models::ConnectionMode::WSL;
    p.isDefault       = true;
    p.wslDistribution = "Ubuntu";
    p.cosmosRootPath  = "/cosmos";

    vm_.saveProfile(p);   // persists and refreshes the profile model/combo

    quickWslBtn_->setEnabled(false);
    connectBtn_->setEnabled(false);
    statusLabel_->setText("Connecting…");
    vm_.connectToProfile(QString::fromStdString(p.id));
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
