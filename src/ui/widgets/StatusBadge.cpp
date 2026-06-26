#include "StatusBadge.h"

namespace OpenC3::UI::Widgets {

StatusBadge::StatusBadge(const QString& text, BadgeStyle style, QWidget* parent)
    : QLabel(text, parent)
{
    setAlignment(Qt::AlignCenter);
    setFixedHeight(20);
    setContentsMargins(8, 2, 8, 2);
    setStyle(style, text);
}

void StatusBadge::setStyle(BadgeStyle style, const QString& text)
{
    if (!text.isEmpty()) setText(text);

    QString bg, fg;
    switch (style) {
        case BadgeStyle::Success: bg = "#1e4d2b"; fg = "#4ec94e"; break;
        case BadgeStyle::Warning: bg = "#3d2e00"; fg = "#cd9100"; break;
        case BadgeStyle::Error:   bg = "#4d1e1e"; fg = "#f1444c"; break;
        case BadgeStyle::Info:    bg = "#1e2e4d"; fg = "#569cd6"; break;
        case BadgeStyle::Neutral: bg = "#2d2d2d"; fg = "#9d9d9d"; break;
    }
    setStyleSheet(QString(
        "QLabel {"
        "  background: %1;"
        "  color: %2;"
        "  border-radius: 10px;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "  padding: 0 8px;"
        "}").arg(bg, fg));
}

} // namespace OpenC3::UI::Widgets
