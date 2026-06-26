#include "SettingsView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QUuid>
#include <QMessageBox>

namespace OpenC3::UI::Views {

SettingsView::SettingsView(
    ViewModels::SettingsViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
    vm_.loadProfiles();
}

void SettingsView::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel("Settings", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Status ────────────────────────────────────────────────────────────────
    statusLabel_ = new QLabel("Status: Disconnected", this);
    statusLabel_->setObjectName("SubLabel");
    root->addWidget(statusLabel_);

    // ── Content splitter ──────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: profile list + buttons
    auto* leftPane   = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    profileList_ = new QListView(leftPane);
    profileList_->setModel(vm_.profileModel());
    profileList_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* listBtns = new QHBoxLayout;
    addBtn_         = new QPushButton("+ Add", leftPane);
    deleteBtn_      = new QPushButton("− Delete", leftPane);
    listBtns->addWidget(addBtn_);
    listBtns->addWidget(deleteBtn_);

    auto* connBtns  = new QHBoxLayout;
    connectBtn_    = new QPushButton("Connect", leftPane);
    disconnectBtn_ = new QPushButton("Disconnect", leftPane);
    connectBtn_->setObjectName("PrimaryButton");
    connBtns->addWidget(connectBtn_);
    connBtns->addWidget(disconnectBtn_);

    leftLayout->addWidget(new QLabel("Connection Profiles:"));
    leftLayout->addWidget(profileList_);
    leftLayout->addLayout(listBtns);
    leftLayout->addLayout(connBtns);

    // Right: profile form
    auto* formGroup  = new QGroupBox("Profile Settings", splitter);
    auto* formLayout = new QFormLayout(formGroup);
    formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    formLayout->setLabelAlignment(Qt::AlignRight);

    nameEdit_        = new QLineEdit(formGroup);
    modeCombo_       = new QComboBox(formGroup);
    modeCombo_->addItems({"WSL", "SSH"});
    wslDistroEdit_   = new QLineEdit(formGroup);
    wslDistroEdit_->setPlaceholderText("Ubuntu");
    hostEdit_        = new QLineEdit(formGroup);
    portEdit_        = new QLineEdit("22", formGroup);
    usernameEdit_    = new QLineEdit(formGroup);
    authMethodCombo_ = new QComboBox(formGroup);
    authMethodCombo_->addItems({"Public Key", "Password"});
    keyPathEdit_     = new QLineEdit(formGroup);
    keyPathEdit_->setPlaceholderText("~/.ssh/id_rsa");
    saveProfileBtn_  = new QPushButton("Save Profile", formGroup);
    saveProfileBtn_->setObjectName("PrimaryButton");

    formLayout->addRow("Name:", nameEdit_);
    formLayout->addRow("Mode:", modeCombo_);
    formLayout->addRow("WSL Distro:", wslDistroEdit_);
    formLayout->addRow("SSH Host:", hostEdit_);
    formLayout->addRow("SSH Port:", portEdit_);
    formLayout->addRow("Username:", usernameEdit_);
    formLayout->addRow("Auth Method:", authMethodCombo_);
    formLayout->addRow("Key Path:", keyPathEdit_);
    formLayout->addRow(saveProfileBtn_);

    splitter->addWidget(leftPane);
    splitter->addWidget(formGroup);
    splitter->setSizes({240, 600});
    root->addWidget(splitter);
}

void SettingsView::bindViewModel()
{
    connect(addBtn_, &QPushButton::clicked, this, &SettingsView::onAddProfile);
    connect(deleteBtn_, &QPushButton::clicked, this, &SettingsView::onDeleteProfile);
    connect(connectBtn_, &QPushButton::clicked, this, &SettingsView::onConnectClicked);
    connect(disconnectBtn_, &QPushButton::clicked, this, &SettingsView::onDisconnectClicked);
    connect(saveProfileBtn_, &QPushButton::clicked, this, [this] {
        vm_.saveProfile(collectProfileForm());
    });

    connect(profileList_, &QListView::activated,
            this, &SettingsView::onProfileSelected);
    connect(profileList_->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, &SettingsView::onProfileSelected);

    connect(&vm_, &ViewModels::SettingsViewModel::connectionStateChanged,
            this, [this](const QString& state) {
                statusLabel_->setText("Status: " + state);
            });
}

void SettingsView::onAddProfile()
{
    Models::ConnectionProfile p;
    p.id   = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    p.name = "New Profile";
    populateProfileForm(p);
}

void SettingsView::onDeleteProfile()
{
    const auto idx = profileList_->currentIndex();
    if (!idx.isValid()) return;

    const QString id = vm_.profileModel()->data(idx, Qt::UserRole).toString();
    if (QMessageBox::question(this, "Delete Profile",
            "Delete this profile?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        vm_.deleteProfile(id);
}

void SettingsView::onConnectClicked()
{
    const auto idx = profileList_->currentIndex();
    if (!idx.isValid()) return;
    const QString id = vm_.profileModel()->data(idx, Qt::UserRole).toString();
    vm_.connectToProfile(id);
}

void SettingsView::onDisconnectClicked()
{
    vm_.disconnect();
}

void SettingsView::onProfileSelected(const QModelIndex& idx)
{
    if (!idx.isValid()) return;
    const auto* p = vm_.profileModel()->profileAt(idx.row());
    if (p) populateProfileForm(*p);
}

void SettingsView::populateProfileForm(const Models::ConnectionProfile& p)
{
    nameEdit_->setText(QString::fromStdString(p.name));
    modeCombo_->setCurrentIndex(static_cast<int>(p.mode));
    wslDistroEdit_->setText(QString::fromStdString(p.wslDistribution));
    hostEdit_->setText(QString::fromStdString(p.host));
    portEdit_->setText(QString::number(p.port));
    usernameEdit_->setText(QString::fromStdString(p.username));
    authMethodCombo_->setCurrentIndex(static_cast<int>(p.authMethod));
    keyPathEdit_->setText(QString::fromStdString(p.privateKeyPath));
}

Models::ConnectionProfile SettingsView::collectProfileForm() const
{
    Models::ConnectionProfile p;
    const auto idx = profileList_->currentIndex();
    if (idx.isValid()) {
        const QString id =
            vm_.profileModel()->data(idx, Qt::UserRole).toString();
        p.id = id.toStdString();
    }
    if (p.id.empty())
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    p.name            = nameEdit_->text().toStdString();
    p.mode            = static_cast<Models::ConnectionMode>(modeCombo_->currentIndex());
    p.wslDistribution = wslDistroEdit_->text().toStdString();
    p.host            = hostEdit_->text().toStdString();
    p.port            = portEdit_->text().toInt();
    p.username        = usernameEdit_->text().toStdString();
    p.authMethod      = static_cast<Models::AuthMethod>(authMethodCombo_->currentIndex());
    p.privateKeyPath  = keyPathEdit_->text().toStdString();
    return p;
}

} // namespace OpenC3::UI::Views
