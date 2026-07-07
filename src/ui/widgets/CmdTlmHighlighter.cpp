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
        add(R"(\b(UINT|UINT8|UINT16|UINT32|UINT64|INT|INT8|INT16|INT32|INT64|)"
            R"(FLOAT|FLOAT32|FLOAT64|STRING|BLOCK|DERIVED)\b)", f);
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

    // ── Ruby ruleset (procedures/*.rb scripts) ────────────────────────────────
    auto addRuby = [this](const QString& pattern, const QTextCharFormat& fmt) {
        rubyRules_.append({ QRegularExpression(pattern), fmt });
    };

    // Ruby keywords — blue bold, matching the COSMOS DSL block-keyword style.
    {
        QTextCharFormat f;
        f.setForeground(QColor("#569CD6"));
        f.setFontWeight(QFont::Bold);
        addRuby(R"(\b(def|end|if|elsif|else|unless|while|until|do|class|module|)"
                R"(return|begin|rescue|ensure|raise|require|require_relative|)"
                R"(then|case|when|break|next|yield|self|nil|true|false|and|or|not)\b)", f);
    }

    // COSMOS script API calls — green, the same weight this app already gives
    // COSMOS-specific identifiers (APPEND_ITEM etc.) elsewhere in this file.
    {
        QTextCharFormat f;
        f.setForeground(QColor("#4EC994"));
        addRuby(R"(\b(cmd|cmd_no_hazardous_check|cmd_no_range_check|tlm|tlm_variable|)"
                R"(wait|wait_check|wait_check_expression|check|check_expression|)"
                R"(check_tolerance|puts|print)\b(?=\s*\()", f);
    }

    // Strings (both quote styles Ruby accepts) — warm-cream.
    {
        QTextCharFormat f;
        f.setForeground(QColor("#D69D85"));
        addRuby(R"("[^"]*"|'[^']*')", f);
    }

    // Numbers — pale green.
    {
        QTextCharFormat f;
        f.setForeground(QColor("#B5CEA8"));
        addRuby(R"(\b-?\d+\.?\d*([eE][+-]?\d+)?\b)", f);
    }

    // Line comments (#…) — muted green italic (applied LAST to override all).
    {
        QTextCharFormat f;
        f.setForeground(QColor("#6A9955"));
        f.setFontItalic(true);
        addRuby(R"(#[^\n]*)", f);
    }
}

void CmdTlmHighlighter::setRubyMode(bool on)
{
    if (rubyMode_ == on)
        return;
    rubyMode_ = on;
    rehighlight();
}

void CmdTlmHighlighter::highlightBlock(const QString& text)
{
    for (const auto& rule : (rubyMode_ ? rubyRules_ : rules_)) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(static_cast<int>(m.capturedStart()),
                      static_cast<int>(m.capturedLength()), rule.format);
        }
    }
}

} // namespace OpenC3::UI::Widgets
