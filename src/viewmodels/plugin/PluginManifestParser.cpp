#include "PluginManifestParser.h"

#include "viewmodels/validation/PluginKeywords.h"
#include "viewmodels/validation/TextTokenizer.h"

namespace OpenC3::ViewModels {

bool PluginManifestParser::isKnownTopLevelKeyword(const QString& keyword)
{
    return Validation::PluginKeywords::topLevel().contains(keyword.toUpper());
}

PluginManifestParseResult PluginManifestParser::parse(const QString& content)
{
    PluginManifestParseResult result;
    const QStringList lines = content.split('\n');

    PluginManifestBlock* currentBlock = nullptr;

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo   = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = Validation::tokenizeConfigLine(trimmed);
        if (toks.isEmpty()) continue;

        const QString kw = toks[0].toUpper();

        // ── Top-level declaration: starts a new block ────────────────────────
        if (Validation::PluginKeywords::topLevel().contains(kw)) {
            currentBlock = nullptr;

            PluginManifestBlock block;
            block.lineNumber = lineNo;

            if (kw == "TARGET") {
                // TARGET <folder> <name> [...]
                block.kind = PluginManifestBlock::Kind::Target;
                if (toks.size() > 1) block.classFileOrFolder = toks[1];
                if (toks.size() > 2) block.name = toks[2].toUpper();
                for (int t = 3; t < toks.size(); ++t) block.extraArgs << toks[t];
            } else if (kw == "INTERFACE" || kw == "ROUTER") {
                // INTERFACE/ROUTER <name> <class_file> <params...>
                block.kind = (kw == "INTERFACE") ? PluginManifestBlock::Kind::Interface
                                                  : PluginManifestBlock::Kind::Router;
                if (toks.size() > 1) block.name = toks[1].toUpper();
                if (toks.size() > 2) block.classFileOrFolder = toks[2];
                for (int t = 3; t < toks.size(); ++t) block.extraArgs << toks[t];
            } else if (kw == "MICROSERVICE") {
                // MICROSERVICE <folder> <name>
                block.kind = PluginManifestBlock::Kind::Microservice;
                if (toks.size() > 1) block.classFileOrFolder = toks[1];
                if (toks.size() > 2) block.name = toks[2].toUpper();
                for (int t = 3; t < toks.size(); ++t) block.extraArgs << toks[t];
            } else if (kw == "TOOL" || kw == "WIDGET") {
                // TOOL/WIDGET carry no fixed positional grammar - pure modifier
                // bags (URL/ICON/CATEGORY/... via PluginKeywords::toolModifiers()).
                block.kind = (kw == "TOOL") ? PluginManifestBlock::Kind::Tool
                                            : PluginManifestBlock::Kind::Widget;
                for (int t = 1; t < toks.size(); ++t) block.extraArgs << toks[t];
            } else if (kw == "VARIABLE") {
                // VARIABLE <name> <default_value> - single-line, no modifiers.
                block.kind = PluginManifestBlock::Kind::Variable;
                if (toks.size() > 1) block.name = toks[1].toUpper();
                for (int t = 2; t < toks.size(); ++t) block.extraArgs << toks[t];

                if (block.name.isEmpty()) {
                    result.diagnostics.append({
                        PluginManifestDiagnostic::Severity::Warning, lineNo,
                        QStringLiteral("VARIABLE has no name")});
                }
                result.blocks.append(block);
                continue;
            } else {
                // NEEDS_DEPENDENCIES / SCRIPT_ENGINE - recognised but not an
                // editable Manifest-tab block kind; skip without diagnostics.
                continue;
            }

            const bool nameRequired = block.kind != PluginManifestBlock::Kind::Tool
                                    && block.kind != PluginManifestBlock::Kind::Widget;
            if (nameRequired && block.name.isEmpty()) {
                result.diagnostics.append({
                    PluginManifestDiagnostic::Severity::Warning, lineNo,
                    QStringLiteral("%1 has no name").arg(kw)});
            }

            result.blocks.append(block);
            // Safe: result.blocks is only appended to on a top-level keyword
            // line, immediately reassigning currentBlock right after (same
            // pointer-into-QVector pattern CmdTlmParser already uses) - no
            // append happens via this pointer in between, only modifier pushes
            // into the pointed-to block's own QVector<modifier>.
            currentBlock = &result.blocks.last();
            continue;
        }

        // ── Modifier line (PROTOCOL, ENV, SECRET, MAP_TARGET, OPTION, ...) ────
        // Recognition of *which* modifiers are valid per context is
        // deliberately left to PluginConfigParser/InterfaceValidator (already
        // wired into the Check step) - this parser accepts any non-top-level
        // line while a block is open, so it stays a thin structural model
        // rather than a second source of truth for keyword validity.
        if (!currentBlock) {
            result.diagnostics.append({
                PluginManifestDiagnostic::Severity::Warning, lineNo,
                QStringLiteral("'%1' is not inside a TARGET/INTERFACE/ROUTER/"
                                "MICROSERVICE/TOOL/WIDGET block").arg(toks[0])});
            continue;
        }

        PluginManifestModifier mod;
        mod.lineNumber = lineNo;
        mod.keyword    = kw;
        for (int t = 1; t < toks.size(); ++t) mod.args << toks[t];
        mod.rawArgsText = mod.args.join(' ');

        currentBlock->modifiers.append(mod);
    }

    return result;
}

} // namespace OpenC3::ViewModels
