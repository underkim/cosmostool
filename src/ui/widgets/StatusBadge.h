#pragma once

#include <QLabel>
#include <QString>

namespace OpenC3::UI::Widgets {

enum class BadgeStyle { Success, Warning, Error, Info, Neutral };

/// A small coloured pill label for displaying status text.
class StatusBadge final : public QLabel {
    Q_OBJECT
public:
    explicit StatusBadge(const QString& text = {},
                         BadgeStyle style    = BadgeStyle::Neutral,
                         QWidget*   parent  = nullptr);

    void setStyle(BadgeStyle style, const QString& text = {});
};

} // namespace OpenC3::UI::Widgets
