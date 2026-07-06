#pragma once

#include "viewmodels/screen/ScreenLayoutParser.h"

#include <QWidget>

class QBoxLayout;

namespace OpenC3::UI::Widgets {

// ── ScreenPreviewWidget ────────────────────────────────────────────────────────
//
// Renders a COSMOS screen definition's *static layout* - real nested widgets
// (QVBoxLayout/QHBoxLayout mirroring VERTICAL(BOX)/HORIZONTAL(BOX), QLabel/
// QPushButton/... for common leaf widgets) so the Workspace Preview tab shows
// an actual pixel layout, not just a table of keywords.
//
// This is NOT a live telemetry viewer: there is no connection, so
// telemetry-bound widgets (VALUE, LABELVALUE, ...) show their bound
// TARGET/PACKET/ITEM name where a live value would normally render. Widget
// kinds this app recognises (ScreenLayoutParser accepts any keyword) but
// doesn't yet render in detail fall back to a labeled placeholder box rather
// than being silently dropped.
class ScreenPreviewWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ScreenPreviewWidget(QWidget* parent = nullptr);

    void setLayoutTree(const ViewModels::ScreenLayoutResult& result);

private:
    QWidget* buildNode(const ViewModels::ScreenWidgetNode& node);
    QWidget* buildContainer(const ViewModels::ScreenWidgetNode& node);
    QWidget* buildLeaf(const ViewModels::ScreenWidgetNode& node);
    QWidget* buildPlaceholder(const ViewModels::ScreenWidgetNode& node);

    QBoxLayout* rootLayout_{nullptr};
};

} // namespace OpenC3::UI::Widgets
