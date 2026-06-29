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
    Severity severity{Severity::Warning};
    int      line{0};
    QString  message;
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

private:
    static QStringList tokenize(const QString& line);
    static bool        isValidDataType(const QString& type);
};

} // namespace OpenC3::ViewModels
