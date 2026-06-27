#include "CmdTlmHighlighter.h"

#include <QColor>
#include <QFont>

namespace OpenC3::UI::Widgets {

CmdTlmHighlighter::CmdTlmHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    auto add = [this](const QString& pattern, const QTextCharFormat& fmt) {
        rules_.append({ QRegularExpression(pattern), fmt });
    };

    // Order matters: later rules overwrite earlier ones.
    // Apply specific ones (data types, strings) after broad keyword matches
    // so they still colour correctly inside long lines.

    // ── COMMAND / TELEMETRY header keywords — blue bold ──────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#569CD6"));
        f.setFontWeight(QFont::Bold);
        add(R"(^\s*\b(COMMAND|TELEMETRY)\b)", f);
    }

    // ── Item-definition keywords — green ─────────────────────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#4EC994"));
        add(R"(^\s*\b(APPEND_ID_PARAMETER|ID_PARAMETER|APPEND_PARAMETER|PARAMETER|)"
            R"(APPEND_ID_ITEM|ID_ITEM|APPEND_ITEM|ITEM)\b)", f);
    }

    // ── Sub-directive keywords — steel-blue ───────────────────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#9CDCFE"));
        add(R"(^\s*\b(STATE|UNITS|FORMAT_STRING|REQUIRED|LIMITS|HAZARDOUS|)"
            R"(ALLOW_SHORT|META|HIDDEN|OVERFLOW|DESCRIPTION|ACCESSOR|)"
            R"(POLY_READ_CONVERSION|POLY_WRITE_CONVERSION|)"
            R"(READ_CONVERSION|WRITE_CONVERSION|DISABLE_MESSAGES)\b)", f);
    }

    // ── Valid data types — orange ─────────────────────────────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#CE9178"));
        add(R"(\b(UINT8|UINT16|UINT32|UINT64|INT8|INT16|INT32|INT64|)"
            R"(FLOAT32|FLOAT64|STRING|BLOCK|DERIVED)\b)", f);
    }

    // ── Endianness — magenta ──────────────────────────────────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#C586C0"));
        add(R"(\b(BIG_ENDIAN|LITTLE_ENDIAN)\b)", f);
    }

    // ── Quoted strings — warm-cream (applied before numbers so "10" stays cream)
    {
        QTextCharFormat f;
        f.setForeground(QColor("#D69D85"));
        add(R"("[^"]*")", f);
    }

    // ── Numbers (int/float/sci-notation) — pale green ─────────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#B5CEA8"));
        add(R"(\b-?\d+\.?\d*([eE][+-]?\d+)?\b)", f);
    }

    // ── Hex literals — same pale green ────────────────────────────────────────
    {
        QTextCharFormat f;
        f.setForeground(QColor("#B5CEA8"));
        add(R"(\b0[xX][0-9A-Fa-f]+\b)", f);
    }

    // ── Line comments (#…) — muted green italic (applied LAST to override all)
    {
        QTextCharFormat f;
        f.setForeground(QColor("#6A9955"));
        f.setFontItalic(true);
        add(R"(#[^\n]*)", f);
    }
}

void CmdTlmHighlighter::highlightBlock(const QString& text)
{
    for (const auto& rule : rules_) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }
}

} // namespace OpenC3::UI::Widgets
