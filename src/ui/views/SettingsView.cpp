#include "SettingsView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFrame>
#include <QLabel>
#include <QFont>
#include <QUuid>
#include <QMessageBox>
#include <QProcess>
#include <QFileDialog>
#include <QIntValidator>

namespace OpenC3::UI::Views {

SettingsView::SettingsView(
    ViewModels::SettingsViewModel& vm, QWidget* parent)
    : QWidget(parent)
    , vm_(vm)
{
    setupUi();
    bindViewModel();
    vm_.loadProfiles();
    refreshWslDistros(); // populate WSL combo on startup
}

// ── UI Construction ───────────────────────────────────────────────────────────

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
    statusLabel_->setObjectName("StatusBanner");
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    // ── First-run connection cards ────────────────────────────────────────────
    auto* startCards = new QHBoxLayout;
    startCards->setSpacing(12);

    auto* quickCard = new QFrame(this);
    quickCard->setObjectName("ActionCard");
    auto* quickCardLayout = new QVBoxLayout(quickCard);
    quickCardLayout->setContentsMargins(14, 12, 14, 12);
    quickCardLayout->setSpacing(6);
    auto* quickTitle = new QLabel("Quick WSL", quickCard);
    quickTitle->setObjectName("CardTitle");
    auto* quickDesc = new QLabel(
        "Best for local testing. Auto-detect WSL and create a /cosmos profile.",
        quickCard);
    quickDesc->setObjectName("SubLabel");
    quickDesc->setWordWrap(true);
    quickWslBtn_ = new QPushButton("Create WSL Profile", quickCard);
    quickWslBtn_->setObjectName("PrimaryButton");
    quickWslBtn_->setToolTip("Auto-detect WSL and create a /cosmos profile.");
    quickCardLayout->addWidget(quickTitle);
    quickCardLayout->addWidget(quickDesc, 1);
    quickCardLayout->addWidget(quickWslBtn_, 0, Qt::AlignLeft);

    auto* customCard = new QFrame(this);
    customCard->setObjectName("ActionCard");
    auto* customCardLayout = new QVBoxLayout(customCard);
    customCardLayout->setContentsMargins(14, 12, 14, 12);
    customCardLayout->setSpacing(6);
    auto* customTitle = new QLabel("Custom Profile", customCard);
    customTitle->setObjectName("CardTitle");
    auto* customDesc = new QLabel(
        "Use SSH, advanced WSL settings, custom paths, or saved credentials.",
        customCard);
    customDesc->setObjectName("SubLabel");
    customDesc->setWordWrap(true);
    addBtn_ = new QPushButton("Create Custom Profile", customCard);
    addBtn_->setToolTip("Create a blank connection profile for SSH or advanced setup.");
    customCardLayout->addWidget(customTitle);
    customCardLayout->addWidget(customDesc, 1);
    customCardLayout->addWidget(addBtn_, 0, Qt::AlignLeft);

    startCards->addWidget(quickCard);
    startCards->addWidget(customCard);
    root->addLayout(startCards);

    // ── Splitter: list | form ─────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left pane — profile list ──────────────────────────────────────────────
    auto* leftPane   = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    profileList_ = new QListView(leftPane);
    profileList_->setModel(vm_.profileModel());
    profileList_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* manageBtns = new QHBoxLayout;
    deleteBtn_ = new QPushButton("Delete", leftPane);
    deleteBtn_->setToolTip("Select a connection profile first.");
    manageBtns->addWidget(deleteBtn_);
    manageBtns->addStretch();

    profileHintLabel_ = new QLabel(
        "Start with the cards above, then select a saved profile to review, connect, or delete it.",
        leftPane);
    profileHintLabel_->setObjectName("SubLabel");
    profileHintLabel_->setWordWrap(true);

    auto* connBtns  = new QHBoxLayout;
    connectBtn_    = new QPushButton("Connect",    leftPane);
    disconnectBtn_ = new QPushButton("Disconnect", leftPane);
    connectBtn_->setObjectName("PrimaryButton");
    connectBtn_->setToolTip("Select a connection profile first.");
    disconnectBtn_->setToolTip("Connect to an OpenC3 environment first.");
    connBtns->addWidget(connectBtn_);
    connBtns->addWidget(disconnectBtn_);

    leftLayout->addWidget(new QLabel("Connection Profiles:", leftPane));
    leftLayout->addWidget(profileList_);
    leftLayout->addWidget(profileHintLabel_);
    leftLayout->addLayout(manageBtns);
    leftLayout->addLayout(connBtns);

    // ── Right pane — profile form ─────────────────────────────────────────────
    auto* formGroup  = new QGroupBox("Profile Settings", splitter);
    auto* formLayout = new QFormLayout(formGroup);
    formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    profileFormStateLabel_ = new QLabel("New profile — save it before connecting.", formGroup);
    profileFormStateLabel_->setObjectName("SubLabel");
    profileFormStateLabel_->setWordWrap(true);
    formLayout->addRow(profileFormStateLabel_);

    // Common fields
    nameEdit_  = new QLineEdit(formGroup);
    modeCombo_ = new QComboBox(formGroup);
    modeCombo_->addItems({"WSL", "SSH"});
    cosmosRootPathEdit_ = new QLineEdit("/cosmos", formGroup);
    cosmosRootPathEdit_->setPlaceholderText("/cosmos or /path/to/openc3.sh");
    cosmosRootPathEdit_->setFont(QFont("Consolas", 10));
    cosmosRootPathEdit_->setToolTip(
        "Remote OpenC3 path used by Infra tools.\n"
        "Enter either the OpenC3 directory or the full openc3.sh path.\n"
        "Examples: /cosmos, /home/user/openc3, /home/user/openc3/openc3.sh");
    detectOpenC3PathBtn_ = new QPushButton("Detect", formGroup);
    detectOpenC3PathBtn_->setToolTip("Search the selected WSL distro for openc3.sh");

    auto* openC3PathRow = new QWidget(formGroup);
    auto* openC3PathLayout = new QHBoxLayout(openC3PathRow);
    openC3PathLayout->setContentsMargins(0, 0, 0, 0);
    openC3PathLayout->addWidget(cosmosRootPathEdit_, 1);
    openC3PathLayout->addWidget(detectOpenC3PathBtn_);

    formLayout->addRow("Name:", nameEdit_);
    formLayout->addRow("Mode:", modeCombo_);
    formLayout->addRow("OpenC3 path:", openC3PathRow);

    // ── Mode-specific stack ───────────────────────────────────────────────────
    modeStack_ = new QStackedWidget(formGroup);

    // Page 0 — WSL
    auto* wslPage   = new QWidget(modeStack_);
    auto* wslLayout = new QFormLayout(wslPage);
    wslLayout->setContentsMargins(0, 8, 0, 0);
    wslLayout->setLabelAlignment(Qt::AlignRight);
    wslLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    wslDistroCombo_ = new QComboBox(wslPage);
    wslRefreshBtn_  = new QPushButton("↻", wslPage);
    wslRefreshBtn_->setFixedWidth(28);
    wslRefreshBtn_->setToolTip("Refresh WSL distribution list");

    auto* wslDistroRow = new QHBoxLayout;
    wslDistroRow->addWidget(wslDistroCombo_);
    wslDistroRow->addWidget(wslRefreshBtn_);
    wslLayout->addRow("WSL Distro:", wslDistroRow);

    auto* wslHint = new QLabel(
        "<small style='color:#858585'>"
        "WSL distributions installed on Windows appear here automatically.<br>"
        "If the list is empty, press the ↻ button, or install one with "
        "<code>wsl --install Ubuntu</code>."
        "</small>", wslPage);
    wslHint->setWordWrap(true);
    wslHint->setTextFormat(Qt::RichText);
    wslLayout->addRow("", wslHint);

    modeStack_->addWidget(wslPage); // index 0

    // Page 1 — SSH
    auto* sshPage   = new QWidget(modeStack_);
    auto* sshLayout = new QFormLayout(sshPage);
    sshLayout->setContentsMargins(0, 8, 0, 0);
    sshLayout->setLabelAlignment(Qt::AlignRight);
    sshLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    hostEdit_       = new QLineEdit(sshPage);
    hostEdit_->setPlaceholderText("192.168.1.100 or hostname");
    portEdit_       = new QLineEdit("22", sshPage);
    portEdit_->setFixedWidth(80);
    portEdit_->setValidator(new QIntValidator(1, 65535, portEdit_));
    usernameEdit_   = new QLineEdit(sshPage);
    usernameEdit_->setPlaceholderText("cosmos");

    // Auth method — order MUST match ConnectionConfig::AuthMethod enum:
    //   Password=0, PublicKey=1
    authMethodCombo_ = new QComboBox(sshPage);
    authMethodCombo_->addItems({"Password", "Public Key"});

    // Password field (shown when auth = Password)
    passwordEdit_ = new QLineEdit(sshPage);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("SSH password");

    // Key path field (shown when auth = Public Key)
    keyPathEdit_ = new QLineEdit(sshPage);
    keyPathEdit_->setPlaceholderText("C:\\Users\\me\\.ssh\\id_rsa");

    auto* keyWidget = new QWidget(sshPage);
    auto* keyRowLayout = new QHBoxLayout(keyWidget);
    keyRowLayout->setContentsMargins(0, 0, 0, 0);
    auto* browseKey = new QPushButton("…", keyWidget);
    browseKey->setFixedWidth(28);
    browseKey->setToolTip("Select a private key file");
    keyRowLayout->addWidget(keyPathEdit_);
    keyRowLayout->addWidget(browseKey);

    sshLayout->addRow("SSH Host:",    hostEdit_);
    sshLayout->addRow("SSH Port:",    portEdit_);
    sshLayout->addRow("Username:",    usernameEdit_);
    sshLayout->addRow("Auth Method:", authMethodCombo_);
    sshLayout->addRow("Password:",    passwordEdit_);
    sshLayout->addRow("Key Path:",    keyWidget);

    // Browse key button
    connect(browseKey, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select Private Key File",
            QDir::homePath() + "/.ssh",
            "Key files (*.pem *.key id_rsa id_ed25519 *);;All files (*)");
        if (!path.isEmpty())
            keyPathEdit_->setText(QDir::toNativeSeparators(path));
    });

    // Show password row or key-path row based on auth method selection
    auto updateAuthFields = [this, sshLayout, keyWidget](int idx) {
        const bool isPassword = (idx == 0); // Password=0, PublicKey=1
        sshLayout->setRowVisible(passwordEdit_, isPassword);
        sshLayout->setRowVisible(keyWidget,     !isPassword);
    };
    connect(authMethodCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, updateAuthFields);
    updateAuthFields(authMethodCombo_->currentIndex());

    modeStack_->addWidget(sshPage); // index 1

    formLayout->addRow(modeStack_);

    // Save buttons
    saveProfileBtn_ = new QPushButton("Save Profile", formGroup);
    saveProfileBtn_->setObjectName("PrimaryButton");
    saveAndConnectBtn_ = new QPushButton("Save && Connect", formGroup);

    auto* saveButtons = new QHBoxLayout;
    saveButtons->addWidget(saveProfileBtn_);
    saveButtons->addWidget(saveAndConnectBtn_);
    saveButtons->addStretch(1);
    formLayout->addRow(saveButtons);

    splitter->addWidget(leftPane);
    splitter->addWidget(formGroup);
    splitter->setChildrenCollapsible(false); // profile list and form stay visible
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    // Wide enough for "+ Add" / "Quick WSL" / "− Delete" side by side without
    // any of them being squeezed below their own text's sizeHint.
    leftPane->setMinimumWidth(260);
    formGroup->setMinimumWidth(360);
    splitter->setSizes({280, 600});
    root->addWidget(splitter);
}

// ── Signals ───────────────────────────────────────────────────────────────────

void SettingsView::bindViewModel()
{
    connect(addBtn_,         &QPushButton::clicked, this, &SettingsView::onAddProfile);
    connect(quickWslBtn_,    &QPushButton::clicked, this, &SettingsView::onQuickWslProfile);
    connect(deleteBtn_,      &QPushButton::clicked, this, &SettingsView::onDeleteProfile);
    connect(connectBtn_,     &QPushButton::clicked, this, &SettingsView::onConnectClicked);
    connect(disconnectBtn_,  &QPushButton::clicked, this, &SettingsView::onDisconnectClicked);
    connect(saveProfileBtn_, &QPushButton::clicked, this, [this] {
        vm_.saveProfile(collectProfileForm());
        updateProfileSelectionUi();
    });
    connect(saveAndConnectBtn_, &QPushButton::clicked, this, [this] {
        const auto profile = collectProfileForm();
        const QString id = QString::fromStdString(profile.id);
        vm_.saveProfile(profile);
        vm_.connectToProfile(id);
        updateProfileSelectionUi();
    });

    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsView::onModeChanged);

    connect(wslRefreshBtn_, &QPushButton::clicked,
            this, &SettingsView::refreshWslDistros);
    connect(detectOpenC3PathBtn_, &QPushButton::clicked,
            this, &SettingsView::detectOpenC3Path);

    connect(profileList_, &QListView::activated,
            this, &SettingsView::onProfileSelected);
    connect(profileList_->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, &SettingsView::onProfileSelected);
    connect(profileList_->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, [this] { updateProfileSelectionUi(); });

    connect(&vm_, &ViewModels::SettingsViewModel::connectionStateChanged,
            this, [this](const QString& state) {
                connectionState_ = state;
                statusLabel_->setText("Status: " + state);
                updateConnectionButtons(state);
                updateActionHints(state);
            });

    // initial stack page
    modeStack_->setCurrentIndex(0);
    detectOpenC3PathBtn_->setEnabled(modeCombo_->currentIndex() == 0);

    // Sync button state with the actual current connection state.
    // SettingsView may be created AFTER a successful connect (e.g. via
    // ConnectionDialog), so the "Connected" signal has already been consumed.
    updateConnectionButtons(vm_.isConnected() ? "Connected" : "Disconnected");
    updateActionHints(vm_.isConnected() ? "Connected" : "Disconnected");
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void SettingsView::onModeChanged(int modeIndex)
{
    modeStack_->setCurrentIndex(modeIndex); // 0=WSL, 1=SSH
    detectOpenC3PathBtn_->setEnabled(modeIndex == 0);
    detectOpenC3PathBtn_->setToolTip(modeIndex == 0
        ? "Search the selected WSL distro for openc3.sh"
        : "Select WSL mode to detect the OpenC3 path automatically.");
}

bool SettingsView::refreshWslDistros()
{
    const QString current = wslDistroCombo_->currentText();
    wslDistroCombo_->clear();

    QProcess proc;
    proc.start("wsl.exe", {"--list", "--quiet"});
    if (!proc.waitForFinished(5000)) {
        wslDistroCombo_->addItem("Ubuntu"); // fallback guess - not a real detection
        return false;
    }

    // wsl.exe --list outputs UTF-16 LE
    const QByteArray raw = proc.readAllStandardOutput();
    QString text;
    if (raw.size() >= 2) {
        text = QString::fromUtf16(
            reinterpret_cast<const char16_t*>(raw.constData()),
            static_cast<qsizetype>(raw.size()) / 2);
    }

    // Filter: remove empty lines, Docker internal distros
    static const QStringList kSkip = {"docker-desktop", "docker-desktop-data"};
    for (const QString& raw_line : text.split('\n')) {
        const QString name = raw_line.trimmed()
                                 .remove(QChar('\r'))
                                 .remove(QChar('\0'));
        if (name.isEmpty() || kSkip.contains(name, Qt::CaseInsensitive))
            continue;
        wslDistroCombo_->addItem(name);
    }

    bool detected = true;
    if (wslDistroCombo_->count() == 0) {
        wslDistroCombo_->addItem("Ubuntu"); // fallback guess - not a real detection
        detected = false;
    }

    // Restore previous selection if still present
    const int idx = wslDistroCombo_->findText(current);
    if (idx >= 0) wslDistroCombo_->setCurrentIndex(idx);

    return detected;
}

void SettingsView::detectOpenC3Path()
{
    if (modeCombo_->currentIndex() != 0) {
        QMessageBox::information(this, "OpenC3 path detection",
            "Automatic detection is currently available for WSL profiles.");
        return;
    }

    const QString distro = wslDistroCombo_->currentText().trimmed();
    if (distro.isEmpty()) {
        QMessageBox::information(this, "OpenC3 path detection",
            "Select a WSL distro first.");
        return;
    }

    detectOpenC3PathBtn_->setEnabled(false);
    detectOpenC3PathBtn_->setText("Detecting...");

    const QString script = R"(for p in \
  /cosmos/openc3.sh \
  /openc3/openc3.sh \
  "$HOME/openc3/openc3.sh" \
  "$HOME/cosmos/openc3.sh"; do
  [ -f "$p" ] && printf '%s\n' "$p" && exit 0
done
find "$HOME" /opt /srv /cosmos /openc3 -maxdepth 5 -type f -name openc3.sh 2>/dev/null | head -n 1)";

    QProcess proc;
    proc.start("wsl.exe", {"-d", distro, "--", "sh", "-lc", script});

    const bool finished = proc.waitForFinished(15000);
    detectOpenC3PathBtn_->setText("Detect");
    detectOpenC3PathBtn_->setEnabled(modeCombo_->currentIndex() == 0);

    if (!finished) {
        proc.kill();
        QMessageBox::warning(this, "OpenC3 path detection",
            "Timed out while searching for openc3.sh.");
        return;
    }

    const QString found =
        QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (proc.exitStatus() == QProcess::NormalExit
        && proc.exitCode() == 0
        && !found.isEmpty()) {
        cosmosRootPathEdit_->setText(found.split('\n').first().trimmed());
        return;
    }

    QMessageBox::information(this, "OpenC3 path detection",
        "Could not find openc3.sh in the selected WSL distro.\n"
        "Enter the OpenC3 directory or openc3.sh path manually.");
}

void SettingsView::onAddProfile()
{
    // Clear the list selection first: collectProfileForm() derives the profile
    // id from the currently selected row, so without this a freshly added
    // profile would be saved over whichever profile is still highlighted.
    // With no selection, collectProfileForm() generates a fresh id instead.
    profileList_->selectionModel()->clear();
    profileList_->selectionModel()->clearCurrentIndex();
    profileList_->setCurrentIndex(QModelIndex());

    Models::ConnectionProfile p;
    p.name = "New Profile";
    populateProfileForm(p);

    updateProfileSelectionUi();
    nameEdit_->setFocus();
    nameEdit_->selectAll();
    updateActionHints(vm_.isConnected() ? "Connected" : "Disconnected");
}

void SettingsView::onQuickWslProfile()
{
    const bool detected = refreshWslDistros();
    QString distro = wslDistroCombo_->currentText().trimmed();
    if (distro.isEmpty())
        distro = QStringLiteral("Ubuntu");

    if (!detected) {
        QMessageBox box(this);
        box.setWindowTitle("No WSL Distro Detected");
        box.setText("Could not detect any installed WSL distribution.");
        box.setInformativeText(
            "You can proceed with \"" + distro + "\" as a guess, "
            "create a custom profile instead, or cancel.");
        auto* useDefaultBtn = box.addButton("Use " + distro + " Defaults", QMessageBox::AcceptRole);
        auto* customBtn     = box.addButton("Create Custom Profile", QMessageBox::ActionRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();

        if (box.clickedButton() == customBtn) {
            onAddProfile();
            return;
        }
        if (box.clickedButton() != useDefaultBtn)
            return; // Cancel (or dialog dismissed) - save nothing
    }

    Models::ConnectionProfile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    p.name = QStringLiteral("WSL (%1)").arg(distro).toStdString();
    p.mode = Models::ConnectionMode::WSL;
    p.wslDistribution = distro.toStdString();
    p.cosmosRootPath = "/cosmos";
    p.isDefault = false;

    const QString profileId = QString::fromStdString(p.id);
    vm_.saveProfile(p);

    for (int row = 0; row < vm_.profileModel()->rowCount(); ++row) {
        const QModelIndex idx = vm_.profileModel()->index(row, 0);
        if (vm_.profileModel()->data(idx, Qt::UserRole).toString() == profileId) {
            profileList_->setCurrentIndex(idx);
            profileList_->selectionModel()->select(
                idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            populateProfileForm(p);
            break;
        }
    }

    statusLabel_->setText(
        QStringLiteral("Status: WSL profile created for %1 — review it, then Save & Connect.")
            .arg(distro));
    updateProfileSelectionUi();
    updateActionHints(vm_.isConnected() ? "Connected" : "Disconnected");
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
    if (!idx.isValid()) {
        updateProfileSelectionUi();
        return;
    }
    const QString id = vm_.profileModel()->data(idx, Qt::UserRole).toString();
    vm_.connectToProfile(id);
}

void SettingsView::onDisconnectClicked()
{
    vm_.disconnect();
}

void SettingsView::onProfileSelected(const QModelIndex& idx)
{
    if (!idx.isValid()) {
        updateActionHints(vm_.isConnected() ? "Connected" : "Disconnected");
        return;
    }
    const auto* p = vm_.profileModel()->profileAt(idx.row());
    if (p) populateProfileForm(*p);
    updateActionHints(vm_.isConnected() ? "Connected" : "Disconnected");
}

// ── Form helpers ──────────────────────────────────────────────────────────────

void SettingsView::populateProfileForm(const Models::ConnectionProfile& p)
{
    nameEdit_->setText(QString::fromStdString(p.name));
    cosmosRootPathEdit_->setText(QString::fromStdString(
        p.cosmosRootPath.empty() ? "/cosmos" : p.cosmosRootPath));

    const int modeIdx = static_cast<int>(p.mode);
    modeCombo_->setCurrentIndex(modeIdx);
    modeStack_->setCurrentIndex(modeIdx);

    // WSL fields
    const QString distro = QString::fromStdString(p.wslDistribution);
    const int di = wslDistroCombo_->findText(distro);
    if (di >= 0)
        wslDistroCombo_->setCurrentIndex(di);
    else if (!distro.isEmpty()) {
        wslDistroCombo_->addItem(distro);
        wslDistroCombo_->setCurrentText(distro);
    }

    // SSH fields
    hostEdit_->setText(QString::fromStdString(p.host));
    portEdit_->setText(QString::number(p.port > 0 ? p.port : 22));
    usernameEdit_->setText(QString::fromStdString(p.username));
    authMethodCombo_->setCurrentIndex(static_cast<int>(p.authMethod));
    passwordEdit_->setText(QString::fromStdString(p.password));
    keyPathEdit_->setText(QString::fromStdString(p.privateKeyPath));
}

Models::ConnectionProfile SettingsView::collectProfileForm() const
{
    Models::ConnectionProfile p;

    const auto idx = profileList_->currentIndex();
    if (idx.isValid())
        p.id = vm_.profileModel()->data(idx, Qt::UserRole).toString().toStdString();
    if (p.id.empty())
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    p.name           = nameEdit_->text().toStdString();
    p.mode           = static_cast<Models::ConnectionMode>(modeCombo_->currentIndex());
    p.cosmosRootPath = cosmosRootPathEdit_->text().trimmed().toStdString();
    if (p.cosmosRootPath.empty()) p.cosmosRootPath = "/cosmos";

    // WSL
    p.wslDistribution = wslDistroCombo_->currentText().toStdString();

    // SSH
    p.host           = hostEdit_->text().toStdString();
    p.port           = portEdit_->text().toInt();
    p.username       = usernameEdit_->text().toStdString();
    p.authMethod     = static_cast<Models::AuthMethod>(authMethodCombo_->currentIndex());
    p.password       = passwordEdit_->text().toStdString();
    p.privateKeyPath = keyPathEdit_->text().toStdString();

    return p;
}

void SettingsView::updateConnectionButtons(const QString& state)
{
    const bool connected  = (state == "Connected");
    const bool connecting = (state == "Connecting");

    connectBtn_->setVisible(!connected);
    disconnectBtn_->setVisible(connected);

    const bool hasSelectedProfile = profileList_->currentIndex().isValid();

    connectBtn_->setEnabled(!connected && !connecting && hasSelectedProfile);
    if (!hasSelectedProfile)
        connectBtn_->setToolTip("Select a saved profile before connecting.");
    else if (connecting)
        connectBtn_->setToolTip("A connection attempt is already in progress.");
    else
        connectBtn_->setToolTip(QString());

    disconnectBtn_->setEnabled(connected && !connecting);
}

void SettingsView::updateProfileSelectionUi()
{
    const bool hasSelectedProfile = profileList_->currentIndex().isValid();
    if (profileFormStateLabel_)
        profileFormStateLabel_->setVisible(!hasSelectedProfile);

    updateConnectionButtons(connectionState_);
    updateActionHints(connectionState_);
}

void SettingsView::updateActionHints(const QString& state)
{
    const bool connected = (state == "Connected");
    const bool connecting = (state == "Connecting");
    const bool hasProfile = profileList_->currentIndex().isValid();
    const QString connectReason = "Connect to an OpenC3 environment first.";
    const QString profileReason = "Select a connection profile first.";

    connectBtn_->setToolTip(!hasProfile ? profileReason
        : (connecting ? "Connection is already in progress." : "Connect to the selected OpenC3 environment."));
    disconnectBtn_->setToolTip(connected ? "Disconnect from the current OpenC3 environment." : connectReason);
    deleteBtn_->setToolTip(hasProfile ? "Delete the selected connection profile." : profileReason);
    quickWslBtn_->setToolTip("Auto-detect WSL and create a /cosmos profile.");
    saveProfileBtn_->setToolTip("Save the current connection profile settings.");

    if (profileHintLabel_) {
        profileHintLabel_->setText(hasProfile
            ? "Profile selected. Review it, save changes if needed, then connect."
            : "New here? Use Quick WSL for local OpenC3, or Custom for SSH/advanced setup.");
    }

    if (!connected && !connecting && !hasProfile)
        statusLabel_->setText("Status: Disconnected — choose Quick WSL or Custom, then connect.");
    else if (!connected && !connecting && hasProfile)
        statusLabel_->setText("Status: Disconnected — connect when ready.");
}

} // namespace OpenC3::UI::Views
