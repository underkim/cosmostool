#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace OpenC3::ViewModels {

// ── Modifier (one indented line inside a TARGET/INTERFACE/ROUTER/MICROSERVICE/
// TOOL/WIDGET block: PROTOCOL, ENV, SECRET, MAP_TARGET, OPTION, ...) ──────────

struct PluginManifestModifier {
    QString     keyword;
    QStringList args;         // tokens after the keyword
    QString     rawArgsText;  // args re-joined (quoting collapsed) for display/edit
    int         lineNumber{0};
};

// ── Block (one TARGET/INTERFACE/ROUTER/MICROSERVICE/TOOL/WIDGET/VARIABLE
// declaration and its modifier lines) ──────────────────────────────────────────

struct PluginManifestBlock {
    enum class Kind { Target, Interface, Router, Microservice, Tool, Widget, Variable };

    Kind    kind{Kind::Target};
    // TARGET/INTERFACE/ROUTER/MICROSERVICE/VARIABLE's name argument. Empty for
    // TOOL/WIDGET, which carry no fixed name argument in COSMOS's grammar.
    QString name;
    // TARGET's <folder>, INTERFACE/ROUTER's <class_file>, MICROSERVICE's
    // <folder>. Empty for TOOL/WIDGET/VARIABLE.
    QString classFileOrFolder;
    // Remaining positional args after name/classFileOrFolder (INTERFACE's
    // connection params, VARIABLE's default value).
    QStringList extraArgs;
    int     lineNumber{0};
    // Empty for Variable (single-line, no nested modifiers). TOOL/WIDGET are
    // pure modifier bags (name/classFileOrFolder/extraArgs unused).
    QVector<PluginManifestModifier> modifiers;
};

// ── Diagnostic (thin UI cue, not a replacement for PluginConfigParser /
// InterfaceValidator / PluginValidator, which remain the source of truth for
// the Check step) ──────────────────────────────────────────────────────────────

struct PluginManifestDiagnostic {
    enum class Severity { Error, Warning };
    Severity severity{Severity::Warning};
    int      line{0};
    QString  message;
};

struct PluginManifestParseResult {
    QVector<PluginManifestBlock>      blocks;
    QVector<PluginManifestDiagnostic> diagnostics;

    [[nodiscard]] int errorCount() const {
        int n = 0;
        for (const auto& d : diagnostics)
            if (d.severity == PluginManifestDiagnostic::Severity::Error) ++n;
        return n;
    }
    [[nodiscard]] int warningCount() const {
        return static_cast<int>(diagnostics.size()) - errorCount();
    }
};

// ── Parser ────────────────────────────────────────────────────────────────────
//
// Parses a plugin.txt's TARGET/INTERFACE/ROUTER/MICROSERVICE/TOOL/WIDGET/
// VARIABLE declarations into structured, line-numbered blocks so PluginView's
// Manifest tab can perform surgical line-based edits (the same pattern
// CmdTlmParser/the Structure tab already use for COMMAND/TELEMETRY) - this
// parser intentionally does NOT duplicate PluginConfigParser's structural
// diagnostics (required-arg checks, MAP_TARGET cross-referencing, unknown-
// modifier detection); the Check step already re-validates saved content with
// that existing pipeline, so this parser's own diagnostics stay minimal.

class PluginManifestParser {
public:
    [[nodiscard]] static PluginManifestParseResult parse(const QString& content);

    // True for a recognised top-level keyword (TARGET/INTERFACE/ROUTER/
    // MICROSERVICE/TOOL/WIDGET/VARIABLE/NEEDS_DEPENDENCIES/SCRIPT_ENGINE),
    // delegating to PluginKeywords::topLevel().
    [[nodiscard]] static bool isKnownTopLevelKeyword(const QString& keyword);
};

} // namespace OpenC3::ViewModels
