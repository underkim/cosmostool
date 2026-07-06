#pragma once

#include <QString>
#include <QVector>

namespace OpenC3::ViewModels {

// ── Item (one field inside a COMMAND or TELEMETRY block) ──────────────────────

struct CmdTlmItem {
    QString name;
    QString keyword;
    QString dataType;
    int     bitSize{0};
    int     bitOffset{0};
    int     arrayBitSize{0};
    QString minVal;
    QString maxVal;
    QString defaultVal;
    QString description;
    int     lineNumber{0};
    bool    isId{false};    // APPEND_ID_PARAMETER / APPEND_ID_ITEM
    bool    isArray{false};
    bool    hasExplicitOffset{false};
};

// ── Block (one COMMAND or TELEMETRY definition) ───────────────────────────────

struct CmdTlmBlock {
    enum class Kind { Command, Telemetry };
    Kind    kind{Kind::Command};
    QString targetName;
    QString name;
    QString endianness;
    QString description;
    int     lineNumber{0};
    bool    isSelect{false};   // SELECT_COMMAND / SELECT_TELEMETRY (modifies existing)
    QVector<CmdTlmItem> items;
};

// ── Diagnostic (error or warning from parse/validate) ─────────────────────────

struct CmdTlmDiagnostic {
    enum class Severity { Error, Warning };
    // Which kind of block the finding belongs to. Block-less structural issues
    // (e.g. a keyword outside any block) stay Any so they surface in both the
    // CMD-only and TLM-only views.
    enum class Scope { Any, Command, Telemetry };
    Severity severity{Severity::Warning};
    int      line{0};
    QString  message;
    Scope    scope{Scope::Any};
};

// ── Result (output of parse()) ────────────────────────────────────────────────

struct CmdTlmParseResult {
    QVector<CmdTlmBlock>      blocks;
    QVector<CmdTlmDiagnostic> diagnostics;

    [[nodiscard]] int errorCount() const {
        int n = 0;
        for (const auto& d : diagnostics)
            if (d.severity == CmdTlmDiagnostic::Severity::Error) ++n;
        return n;
    }
    [[nodiscard]] int warningCount() const {
        return static_cast<int>(diagnostics.size()) - errorCount();
    }
};

// ── Parser ────────────────────────────────────────────────────────────────────

class CmdTlmParser {
public:
    [[nodiscard]] static CmdTlmParseResult parse(const QString& content);

    // True for sub-directives (STATE, UNITS, FORMAT_STRING, ...) that modify
    // the immediately-preceding item/parameter rather than defining a new
    // one. Exposed so callers that delete a field's line (PluginView) can
    // also remove its trailing sub-directive lines instead of orphaning
    // them under whatever field ends up above once the parent is gone.
    [[nodiscard]] static bool isSubDirectiveKeyword(const QString& keyword);

private:
    static QStringList tokenize(const QString& line);
    static bool        isValidDataType(const QString& type);
};

} // namespace OpenC3::ViewModels
