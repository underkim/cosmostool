#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>

namespace OpenC3::UI::Dialogs {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("About OpenC3 Developer Toolkit"));
    setMinimumSize(400, 260);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);

    auto* appName = new QLabel(tr("OpenC3 Developer Toolkit"), this);
    QFont f = appName->font(); f.setPointSize(16); f.setBold(true);
    appName->setFont(f);
    appName->setAlignment(Qt::AlignCenter);

    auto* version = new QLabel(tr("Version 0.1.0"), this);
    version->setAlignment(Qt::AlignCenter);
    version->setObjectName("SubLabel");

    auto* desc = new QLabel(tr(
        "A professional desktop toolkit for OpenC3 COSMOS developers.\n\n"
        "Supports WSL and SSH connections to Linux environments\n"
        "running OpenC3 COSMOS 6.10.4."), this);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);

    auto* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setFixedWidth(100);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    layout->addWidget(appName);
    layout->addWidget(version);
    layout->addSpacing(8);
    layout->addWidget(desc);
    layout->addStretch();
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);
}

} // namespace OpenC3::UI::Dialogs
