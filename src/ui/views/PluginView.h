#pragma once

#include <QWidget>
#include <QLabel>

namespace OpenC3::UI::Views {

/// Plugin Manager view. Full implementation in Phase 7.
class PluginView final : public QWidget {
    Q_OBJECT
public:
    explicit PluginView(QWidget* parent = nullptr);
};

} // namespace OpenC3::UI::Views
