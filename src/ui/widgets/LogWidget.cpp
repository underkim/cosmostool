#include "LogWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QFont>

namespace OpenC3::UI::Widgets {

LogWidget::LogWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolBar   = new QWidget(this);
    auto* tbLayout  = new QHBoxLayout(toolBar);
    tbLayout->setContentsMargins(0, 0, 0, 0);

    tailCheckbox_ = new QCheckBox(tr("Auto-scroll"), toolBar);
    tailCheckbox_->setChecked(true);

    auto* clearBtn = new QPushButton(tr("Clear"), toolBar);
    clearBtn->setFixedWidth(60);
    connect(clearBtn, &QPushButton::clicked, this, &LogWidget::clear);

    tbLayout->addWidget(tailCheckbox_);
    tbLayout->addStretch();
    tbLayout->addWidget(clearBtn);

    // ── Text area ─────────────────────────────────────────────────────────────
    textEdit_ = new QPlainTextEdit(this);
    textEdit_->setReadOnly(true);
    textEdit_->setLineWrapMode(QPlainTextEdit::NoWrap);
    textEdit_->setMaximumBlockCount(10000);
    textEdit_->setObjectName("LogTextEdit");

    QFont mono("Consolas", 10);
    mono.setStyleHint(QFont::Monospace);
    textEdit_->setFont(mono);

    layout->addWidget(toolBar);
    layout->addWidget(textEdit_);
}

void LogWidget::appendLine(const QString& line)
{
    textEdit_->appendPlainText(line);
    if (tailCheckbox_->isChecked())
        textEdit_->verticalScrollBar()->setValue(
            textEdit_->verticalScrollBar()->maximum());
}

void LogWidget::setContent(const QString& content)
{
    textEdit_->setPlainText(content);
    if (tailCheckbox_->isChecked())
        textEdit_->verticalScrollBar()->setValue(
            textEdit_->verticalScrollBar()->maximum());
}

void LogWidget::clear()
{
    textEdit_->clear();
}

} // namespace OpenC3::UI::Widgets
