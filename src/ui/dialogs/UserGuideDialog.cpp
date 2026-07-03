#include "UserGuideDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QShortcut>
#include <QKeySequence>
#include <QStyle>

namespace OpenC3::UI::Dialogs {

UserGuideDialog::UserGuideDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("User Guide — OpenC3 Developer Toolkit");
    resize(820, 680);
    setModal(false); // allow user to keep guide open while working

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Search bar ────────────────────────────────────────────────────────────
    auto* searchBar = new QWidget(this);
    searchBar->setObjectName("searchBar");
    auto* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(8, 6, 8, 6);
    searchLayout->setSpacing(6);

    searchEdit_ = new QLineEdit(searchBar);
    searchEdit_->setPlaceholderText("Search…");
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->addAction(
        style()->standardIcon(QStyle::SP_FileDialogContentsView),
        QLineEdit::LeadingPosition);
    searchLayout->addWidget(searchEdit_);

    root->addWidget(searchBar);

    // ── Content ───────────────────────────────────────────────────────────────
    browser_ = new QTextBrowser(this);
    browser_->setOpenExternalLinks(true);
    browser_->setObjectName("helpBrowser");

    QFile f(":/help/userguide.html");
    if (f.open(QFile::ReadOnly | QFile::Text))
        browser_->setHtml(QString::fromUtf8(f.readAll()));
    else
        browser_->setPlainText("Could not load the help file.");

    root->addWidget(browser_);

    // ── Close button ──────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(8, 6, 8, 6);
    auto* closeBtn = new QPushButton("Close", footer);
    closeBtn->setFixedWidth(80);
    footerLayout->addStretch();
    footerLayout->addWidget(closeBtn);
    root->addWidget(footer);

    connect(closeBtn,   &QPushButton::clicked, this, &QDialog::accept);
    connect(searchEdit_, &QLineEdit::textChanged, this, &UserGuideDialog::onSearch);

    // Esc closes
    auto* esc = new QShortcut(QKeySequence::Cancel, this);
    connect(esc, &QShortcut::activated, this, &QDialog::accept);
}

void UserGuideDialog::onSearch(const QString& text)
{
    if (text.isEmpty()) {
        // Reset highlight by reloading
        browser_->find(""); // clears selection
        return;
    }
    // Jump to first match; Ctrl+G / repeat calls cycle through
    browser_->find(text);
}

} // namespace OpenC3::UI::Dialogs
