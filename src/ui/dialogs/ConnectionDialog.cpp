#include "ConnectionDialog.h"

#include "models/ConnectionProfile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QUuid>
#include <QProcess>
#include <QStringList>

namespace OpenC3::UI::Dialogs {

namespace {

QStringList detectWslDistros()
{
    QStringList distros;

    QProcess proc;
    proc.start(QStringLiteral("wsl.exe"),
               QStringList{QStringLiteral("--list"), QStringLiteral("--quiet")});
    if (!proc.waitForFinished(5000)
        || proc.exitStatus() != QProcess::NormalExit
        || proc.exitCode() != 0) {
        return distros;
    }

    const QByteArray raw = proc.readAllStandardOutput();
    QString text;
    if (raw.size() >= 2) {
        text = QString::fromUtf16(
            reinterpret_cast<const char16_t*>(raw.constData()),
            static_cast<qsizetype>(raw.size()) / 2);
    }

    static const QStringList kSkip = {"docker-desktop", "docker-desktop-data"};
    for (QString name : text.split('\n')) {
        name = name.trimmed()
                   .remove(QChar('\r'))
                   .remove(QChar('\0'))
                   .remove(QChar(0xfeff));
        if (name.isEmpty() || kSkip.contains(name, Qt::CaseInsensitive))
            continue;
        distros << name;
    }

    return distros;
}

QString connectionStateText(Services::ConnectionState state, const QString& errorMessage)
{
    switch (state) {
        case Services::ConnectionState::Connected:    return ConnectionDialog::tr("Connected");
        case Services::ConnectionState::Connecting:   return ConnectionDialog::tr("Connecting");
        case Services::ConnectionState::Disconnected: return ConnectionDialog::tr("Disconnected");
        case Services::ConnectionState::Error:        return ConnectionDialog::tr("Error: %1").arg(errorMessage);
    }
    return {};
}

} // namespace

ConnectionDialog::ConnectionDialog(
    ViewModels::SettingsViewModel& vm, QWidget* parent)
    : QDialog(parent)
    , vm_(vm)
{
    setWindowTitle(tr("Connect to OpenC3 Environment"));
    // Minimum (not fixed) so long/translated status text can grow the dialog
    // instead of being clipped.
    setMinimumSize(420, 180);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    layout->addWidget(new QLabel(tr("Select connection profile:"), this));

    profileCombo_ = new QComboBox(this);
    profileCombo_->setModel(vm_.profileModel());
    layout->addWidget(profileCombo_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("SubLabel");
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    quickWslBtn_ = new QPushButton(tr("Create WSL profile && Connect"), this);
    connect(quickWslBtn_, &QPushButton::clicked,
            this, &ConnectionDialog::onCreateWslProfile);
    layout->addWidget(quickWslBtn_);

    auto* btnRow  = new QHBoxLayout;
    connectBtn_   = new QPushButton(tr("Connect"), this);
    connectBtn_->setObjectName("PrimaryButton");
    auto* skipBtn = new QPushButton(tr("Skip"), this);
    connect(connectBtn_, &QPushButton::clicked, this, &ConnectionDialog::onConnectClicked);
    connect(skipBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addStretch();
    btnRow->addWidget(skipBtn);
    btnRow->addWidget(connectBtn_);
    layout->addLayout(btnRow);

    connect(&vm_, &ViewModels::SettingsViewModel::connectionStateChanged,
            this, [this](Services::ConnectionState state, const QString& errorMessage) {
                statusLabel_->setText(connectionStateText(state, errorMessage));
                const bool connecting = (state == Services::ConnectionState::Connecting);
                const bool hasProfiles = (profileCombo_->count() > 0);
                connectBtn_->setEnabled(hasProfiles && !connecting);
                quickWslBtn_->setEnabled(!quickWslDistro_.isEmpty() && !connecting);
                if (state == Services::ConnectionState::Connected) accept();
            });

    vm_.loadProfiles();

    // Pre-select the default profile (if any) but wait for the user to
    // explicitly confirm the connection. If fast startup is desired, add a
    // separate "Auto-connect default profile" setting instead of connecting
    // implicitly from this dialog.
    bool selectedDefaultProfile = false;
    const int n = profileCombo_->count();
    for (int i = 0; i < n; ++i) {
        const auto* p = vm_.profileModel()->profileAt(i);
        if (p && p->isDefault) {
            profileCombo_->setCurrentIndex(i);
            selectedDefaultProfile = true;
            break;
        }
    }

    if (n == 0) {
        const QStringList distros = detectWslDistros();
        if (!distros.isEmpty()) {
            quickWslDistro_ = distros.first();
            quickWslBtn_->setText(
                tr("Create WSL profile (%1) && Connect").arg(quickWslDistro_));
            quickWslBtn_->setToolTip(
                tr("Creates a default WSL profile (%1, /cosmos) and connects to it.\n"
                               "You can fine-tune it later under Settings.")
                    .arg(quickWslDistro_));
            statusLabel_->setText(
                tr("No saved profiles yet. Click \"Create WSL profile & Connect\" "
                "to use the detected WSL distro, or Skip to explore first."));
        } else {
            quickWslBtn_->setEnabled(false);
            statusLabel_->setText(
                tr("No saved profiles and no WSL distro detected. Install one with "
                "wsl --install Ubuntu, or Skip and configure SSH/WSL in Settings."));
        }
        connectBtn_->setEnabled(false);
    } else {
        quickWslBtn_->setVisible(false);
        if (selectedDefaultProfile) {
            statusLabel_->setText(
                tr("Default profile selected. Click Connect to continue."));
        }
    }
}

void ConnectionDialog::onCreateWslProfile()
{
    if (quickWslDistro_.isEmpty()) {
        const QStringList distros = detectWslDistros();
        if (distros.isEmpty()) {
            quickWslBtn_->setEnabled(false);
            statusLabel_->setText(tr(
                "No WSL distro detected. Install one with wsl --install Ubuntu, "
                "or Skip and configure SSH/WSL in Settings."));
            return;
        }
        quickWslDistro_ = distros.first();
    }

    Models::ConnectionProfile p;
    p.id              = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    p.name            = QStringLiteral("WSL (%1)").arg(quickWslDistro_).toStdString();
    p.mode            = Models::ConnectionMode::WSL;
    p.isDefault       = false;
    p.wslDistribution = quickWslDistro_.toStdString();
    p.cosmosRootPath  = "/cosmos";

    const QString profileId = QString::fromStdString(p.id);
    vm_.saveProfile(p);
    vm_.setDefaultProfile(profileId);

    quickWslBtn_->setEnabled(false);
    connectBtn_->setEnabled(false);
    statusLabel_->setText(tr("Connecting..."));
    vm_.connectToProfile(profileId);
}

void ConnectionDialog::onConnectClicked()
{
    const auto idx = profileCombo_->currentIndex();
    if (idx < 0) return;
    const auto* p = vm_.profileModel()->profileAt(idx);
    if (!p) return;
    connectBtn_->setEnabled(false);
    statusLabel_->setText(tr("Connecting..."));
    vm_.connectToProfile(QString::fromStdString(p->id));
}

} // namespace OpenC3::UI::Dialogs
