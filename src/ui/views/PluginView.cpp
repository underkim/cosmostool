#include "PluginView.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

namespace OpenC3::UI::Views {

PluginView::PluginView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Plugin Manager", this);
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf); title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto* placeholder = new QLabel(
        "Plugin Manager — coming in Phase 7.\n\n"
        "Will support:\n"
        "  • Plugin Wizard\n"
        "  • Plugin Validator\n"
        "  • Install / Remove / Backup\n"
        "  • Version compatibility check",
        this);
    placeholder->setObjectName("SubLabel");
    placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(placeholder);
    layout->addStretch();
}

} // namespace OpenC3::UI::Views
