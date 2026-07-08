#include "ScreenParser.h"
#include "TextTokenizer.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <utility>

namespace OpenC3::ViewModels::Validation {

namespace {

// Layout widgets that open a region which must be closed with END.
const QSet<QString>& containerWidgets()
{
    static const QSet<QString> kContainers = {
        "VERTICAL", "VERTICALBOX",
        "HORIZONTAL", "HORIZONTALBOX",
        "MATRIXBYCOLUMNS",
        "SCROLLWINDOW",
        "TABBOOK", "TABITEM",
        "CANVAS",
        "RADIOGROUP",
    };
    return kContainers;
}

// Settings / modifier keywords that attach to the preceding widget or screen.
const QSet<QString>& settingKeywords()
{
    static const QSet<QString> kSettings = {
        "SETTING", "SUBSETTING",
        "GLOBAL_SETTING", "GLOBAL_SUBSETTING",
    };
    return kSettings;
}

// Known leaf widgets and the minimum number of *arguments* (tokens after the
// widget keyword) they require. Widgets bound to telemetry generally need at
// least TARGET PACKET ITEM. Widgets not listed here are still accepted but are
// reported as unknown so typos are caught.
const QHash<QString, int>& knownWidgets()
{
    static const QHash<QString, int> kWidgets = {
        { "LABEL", 1 },
        { "TITLE", 1 },
        { "SPACER", 0 },
        { "HORIZONTALLINE", 0 },
        { "VALUE", 3 },
        { "LABELVALUE", 3 },
        { "LABELVALUEDESC", 3 },
        { "LABELVALUELIMITSBAR", 3 },
        { "LABELVALUERANGEBAR", 3 },
        { "LABELLED", 3 },
        { "FORMATVALUE", 3 },
        { "ARRAY", 3 },
        { "BLOCK", 3 },
        { "LED", 3 },
        { "LIMITSBAR", 3 },
        { "LIMITSCOLUMN", 3 },
        { "RANGEBAR", 3 },
        { "RANGECOLUMN", 3 },
        { "PROGRESSBAR", 3 },
        { "SPARKLINE", 3 },
        { "LINEGRAPH", 3 },
        { "TEXTFIELD", 0 },
        { "TEXTBOX", 0 },
        { "BUTTON", 2 },
        { "CHECKBUTTON", 1 },
        { "RADIOBUTTON", 1 },
        { "COMBOBOX", 1 },
        { "IMAGE", 1 },
        { "IMAGEVIEWER", 1 },
        { "SCREENSHOTBUTTON", 0 },
    };
    return kWidgets;
}

bool isIntegerOrAuto(const QString& token)
{
    if (token.compare(QStringLiteral("AUTO"), Qt::CaseInsensitive) == 0)
        return true;
    bool ok = false;
    (void)token.toInt(&ok);
    return ok;
}

} // namespace

ValidationReport ScreenParser::parse(const QString& content)
{
    ValidationReport report;
    const QStringList lines = content.split('\n');

    struct OpenContainer {
        QString widget;
        int     line{0};
    };
    QVector<OpenContainer> stack;

    bool sawHeader     = false;
    bool sawWidget     = false;
    bool firstMeaning  = true;

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = tokenizeConfigLine(trimmed);
        if (toks.isEmpty())
            continue;

        const QString kw = toks[0].toUpper();

        // ── SCREEN header ────────────────────────────────────────────────────
        if (kw == "SCREEN") {
            if (sawHeader) {
                report.add(Diagnostic::warning(
                    lineNo, QStringLiteral("Duplicate SCREEN header"),
                    QStringLiteral("screen.header.duplicate"),
                    QStringLiteral("A screen file should contain exactly one SCREEN line.")));
            }
            sawHeader    = true;
            firstMeaning = false;

            if (toks.size() < 4) {
                report.add(Diagnostic::warning(
                    lineNo,
                    QStringLiteral("SCREEN expects <width> <height> <polling_period>"),
                    QStringLiteral("screen.header.args"),
                    QStringLiteral("Example: SCREEN AUTO AUTO 1.0")));
            } else {
                if (!isIntegerOrAuto(toks[1]) || !isIntegerOrAuto(toks[2])) {
                    report.add(Diagnostic::warning(
                        lineNo,
                        QStringLiteral("SCREEN width/height should be an integer or AUTO"),
                        QStringLiteral("screen.header.size")));
                }
                bool okPoll = false;
                (void)toks[3].toDouble(&okPoll);
                if (!okPoll) {
                    report.add(Diagnostic::warning(
                        lineNo,
                        QStringLiteral("SCREEN polling period '%1' is not a number").arg(toks[3]),
                        QStringLiteral("screen.header.polling")));
                }
            }
            continue;
        }

        // First meaningful line must be the SCREEN header.
        if (firstMeaning) {
            firstMeaning = false;
            if (!sawHeader) {
                report.add(Diagnostic::error(
                    lineNo,
                    QStringLiteral("Screen definition must begin with a SCREEN line"),
                    QStringLiteral("screen.header.missing"),
                    QStringLiteral("Add a header such as: SCREEN AUTO AUTO 1.0")));
                sawHeader = true; // avoid repeating the same error for later lines
            }
        }

        // ── END closes the most recent layout widget ─────────────────────────
        if (kw == "END") {
            if (stack.isEmpty()) {
                report.add(Diagnostic::error(
                    lineNo, QStringLiteral("END without a matching layout widget"),
                    QStringLiteral("screen.end.unmatched"),
                    QStringLiteral("Remove this END or add the opening layout widget.")));
            } else {
                stack.removeLast();
            }
            continue;
        }

        // ── Setting / modifier keywords ──────────────────────────────────────
        if (settingKeywords().contains(kw)) {
            if (!sawWidget) {
                report.add(Diagnostic::warning(
                    lineNo,
                    QStringLiteral("'%1' appears before any widget").arg(toks[0]),
                    QStringLiteral("screen.setting.orphan")));
            }
            continue;
        }

        // ── Widget line (possibly NAMED_WIDGET <name> <WIDGET> ...) ──────────
        QString widget    = kw;
        int     argOffset = 1; // index of first argument after the widget token

        if (kw == "NAMED_WIDGET") {
            if (toks.size() < 3) {
                report.add(Diagnostic::warning(
                    lineNo,
                    QStringLiteral("NAMED_WIDGET expects a name and a widget type"),
                    QStringLiteral("screen.namedwidget.args"),
                    QStringLiteral("Example: NAMED_WIDGET temp VALUE INST HEALTH_STATUS TEMP1")));
                continue;
            }
            widget    = toks[2].toUpper();
            argOffset = 3;
        }

        sawWidget = true;

        if (containerWidgets().contains(widget)) {
            stack.append(OpenContainer{ widget, lineNo });
            continue;
        }

        const auto it = knownWidgets().constFind(widget);
        if (it != knownWidgets().constEnd()) {
            const int provided = static_cast<int>(toks.size()) - argOffset;
            if (provided < it.value()) {
                report.add(Diagnostic::warning(
                    lineNo,
                    QStringLiteral("Widget %1 expects at least %2 argument(s) but got %3")
                        .arg(widget).arg(it.value()).arg(provided),
                    QStringLiteral("screen.widget.args")));
            }
            continue;
        }

        // Unknown widget — flag as a warning so typos are visible without
        // failing valid screens that use widgets we don't model yet. Report
        // `widget` (the actual widget type), not toks[0]: for a NAMED_WIDGET
        // line toks[0] is always the literal "NAMED_WIDGET" wrapper, which
        // would misname the offending keyword as "NAMED_WIDGET" instead of
        // the unrecognized type it wraps.
        report.add(Diagnostic::warning(
            lineNo,
            QStringLiteral("Unknown widget or keyword '%1'").arg(widget),
            QStringLiteral("screen.widget.unknown")));
    }

    // ── Unclosed layout widgets ──────────────────────────────────────────────
    for (const auto& open : stack) {
        report.add(Diagnostic::error(
            open.line,
            QStringLiteral("Layout widget '%1' is never closed with END").arg(open.widget),
            QStringLiteral("screen.end.missing"),
            QStringLiteral("Add an END line to close this layout.")));
    }

    if (!sawHeader && lines.size() > 0 && report.diagnostics.isEmpty()) {
        report.add(Diagnostic::warning(
            0, QStringLiteral("Empty or non-screen file"),
            QStringLiteral("screen.empty")));
    }

    return report;
}

} // namespace OpenC3::ViewModels::Validation
