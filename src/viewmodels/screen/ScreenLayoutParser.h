#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace OpenC3::ViewModels {

// ── ScreenWidgetNode (one widget line in a COSMOS screen definition) ─────────
//
// Unlike ScreenParser (src/viewmodels/validation/ScreenParser.h), which only
// checks structure/syntax and discards everything it reads, this node type
// is retained in a real tree so a renderer can walk it (see
// src/ui/widgets/ScreenPreviewWidget.h).

struct ScreenWidgetNode {
    QString keyword;           // TITLE, LABEL, VALUE, VERTICALBOX, BUTTON, ...
    QString namedWidgetName;   // set for NAMED_WIDGET <name> <TYPE> ...; else empty
    QStringList args;          // tokens after the widget keyword
    bool isContainer{false};
    QVector<ScreenWidgetNode> children;  // only populated when isContainer
    int lineNumber{0};
};

struct ScreenLayoutResult {
    QString screenWidth;    // "AUTO" or a number, from the SCREEN header
    QString screenHeight;
    QVector<ScreenWidgetNode> roots;   // top-level widgets (implicit root container)
    bool hasHeader{false};
};

// ── Parser ────────────────────────────────────────────────────────────────────
//
// Builds a best-effort widget tree from a screen definition file for the
// Preview tab's renderer - not a validator (see ScreenParser for that): a
// stray END is ignored, an unclosed container's children still attach to it,
// and unrecognised keywords still become leaf nodes so the preview can render
// a placeholder for them instead of silently dropping content.

class ScreenLayoutParser {
public:
    [[nodiscard]] static ScreenLayoutResult parse(const QString& content);
};

} // namespace OpenC3::ViewModels
