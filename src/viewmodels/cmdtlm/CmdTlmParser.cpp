#include "CmdTlmParser.h"

#include <QSet>
#include <QStringList>

namespace OpenC3::ViewModels {

// ── Constants ─────────────────────────────────────────────────────────────────

static const QSet<QString> kValidDataTypes = {
    "INT", "UINT", "FLOAT",
    "INT8", "INT16", "INT32", "INT64",
    "UINT8", "UINT16", "UINT32", "UINT64",
    "FLOAT32", "FLOAT64",
    "STRING", "BLOCK", "DERIVED"
};

static const QSet<QString> kItemKeywords = {
    "APPEND_PARAMETER",       "PARAMETER",
    "APPEND_ID_PARAMETER",    "ID_PARAMETER",
    "APPEND_ITEM",            "ITEM",
    "APPEND_ID_ITEM",         "ID_ITEM",
    "APPEND_ARRAY_PARAMETER", "ARRAY_PARAMETER",
    "APPEND_ARRAY_ITEM",      "ARRAY_ITEM"
};

// Sub-directives that appear inside blocks but don't define new items.
static const QSet<QString> kSubKeywords = {
    "STATE", "UNITS", "FORMAT_STRING", "REQUIRED",
    "MINIMUM_VALUE", "MAXIMUM_VALUE", "DEFAULT_VALUE",
    "DESCRIPTION", "META", "HIDDEN", "OVERFLOW",
    "LIMITS", "LIMITS_RESPONSE",
    "POLY_READ_CONVERSION", "POLY_WRITE_CONVERSION",
    "READ_CONVERSION", "WRITE_CONVERSION",
    "GENERIC_READ_CONVERSION_START", "GENERIC_WRITE_CONVERSION_START",
    "GENERIC_READ_CONVERSION_END",   "GENERIC_WRITE_CONVERSION_END",
    "ALLOW_SHORT", "HAZARDOUS", "DISABLE_MESSAGES",
    "ACCESSOR", "TEMPLATE", "VARIABLE_BIT_SIZE",
    "IGNORE_OVERLAP", "CCSDS_VER",
    "LIMITS_GROUP", "LIMITS_GROUP_ITEM",
    // SELECT_*/DELETE_* edit an existing packet (used after SELECT_COMMAND /
    // SELECT_TELEMETRY); PROCESSOR/RELATED_ITEM/KEY/OBFUSCATE/VIRTUAL are
    // additional packet/item modifiers. They are valid COSMOS keywords, so
    // recognise them here rather than flagging them as unknown.
    "SELECT_PARAMETER", "SELECT_ITEM",
    "DELETE_PARAMETER", "DELETE_ITEM",
    "PROCESSOR", "RELATED_ITEM", "KEY",
    "OBFUSCATE", "VIRTUAL"
};

// Map a block kind to the diagnostic scope so CMD-only / TLM-only views can
// filter findings to the blocks they care about.
static CmdTlmDiagnostic::Scope scopeOf(CmdTlmBlock::Kind kind)
{
    return kind == CmdTlmBlock::Kind::Command ? CmdTlmDiagnostic::Scope::Command
                                              : CmdTlmDiagnostic::Scope::Telemetry;
}

// ── Tokenizer (handles quoted strings) ───────────────────────────────────────

QStringList CmdTlmParser::tokenize(const QString& line)
{
    QStringList tokens;
    QChar quoteChar;
    QString current;

    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line[i];
        if (c == '"' || c == '\'') {
            if (!quoteChar.isNull() && c == quoteChar) {
                tokens.append(current);
                current.clear();
                quoteChar = QChar{};
            } else if (quoteChar.isNull()) {
                if (!current.isEmpty()) {
                    tokens.append(current);
                    current.clear();
                }
                quoteChar = c;
            } else {
                current += c;
            }
        } else if ((c == ' ' || c == '\t') && quoteChar.isNull()) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

bool CmdTlmParser::isValidDataType(const QString& type)
{
    return kValidDataTypes.contains(type.toUpper());
}

bool CmdTlmParser::isSubDirectiveKeyword(const QString& keyword)
{
    return kSubKeywords.contains(keyword.toUpper());
}

// ── parse() ───────────────────────────────────────────────────────────────────

CmdTlmParseResult CmdTlmParser::parse(const QString& content)
{
    CmdTlmParseResult result;
    const QStringList lines = content.split('\n');

    CmdTlmBlock* currentBlock = nullptr;

    for (int i = 0; i < lines.size(); ++i) {
        const int    lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = tokenize(trimmed);
        if (toks.isEmpty()) continue;

        const QString kw = toks[0].toUpper();

        // ── COMMAND / TELEMETRY / SELECT_* block header ──────────────────────
        if (kw == "COMMAND" || kw == "TELEMETRY"
            || kw == "SELECT_COMMAND" || kw == "SELECT_TELEMETRY") {
            CmdTlmBlock block;
            block.kind       = kw.contains("COMMAND") ? CmdTlmBlock::Kind::Command
                                                       : CmdTlmBlock::Kind::Telemetry;
            block.isSelect   = kw.startsWith("SELECT");
            block.lineNumber = lineNo;

            if (toks.size() >= 2) block.targetName = toks[1].toUpper();
            if (toks.size() >= 3) block.name       = toks[2].toUpper();

            // SELECT_* only references an existing packet (target + packet);
            // a full definition also carries endianness and a description.
            if (!block.isSelect) {
                if (toks.size() >= 4) block.endianness = toks[3].toUpper();
                if (toks.size() >= 5) block.description= toks[4];

                if (!block.endianness.isEmpty()
                    && block.endianness != "BIG_ENDIAN"
                    && block.endianness != "LITTLE_ENDIAN") {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Warning, lineNo,
                        QString("Unrecognised endianness '%1'").arg(block.endianness),
                        scopeOf(block.kind)
                    });
                }
            }

            if (block.targetName.isEmpty() || block.name.isEmpty()) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("%1 is missing target or packet name").arg(kw),
                    scopeOf(block.kind)
                });
            }

            result.blocks.append(block);
            currentBlock = &result.blocks.last();
            continue;
        }

        // ── Item keywords ─────────────────────────────────────────────────────
        if (kItemKeywords.contains(kw)) {
            if (!currentBlock) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("'%1' outside of COMMAND/TELEMETRY block").arg(kw)
                });
                continue;
            }

            CmdTlmItem item;
            item.lineNumber = lineNo;
            item.keyword    = kw;
            item.isId       = kw.contains("ID_");
            item.isArray    = kw.contains("ARRAY");

            const bool isParam  = kw.contains("PARAMETER");
            const bool isAppend = kw.startsWith("APPEND");
            item.hasExplicitOffset = !isAppend;

            // COSMOS grammar (positional). APPEND_* omit the leading bit_offset:
            //   [APPEND_]PARAMETER    <name> [bit_offset] <bit_size> <type> <min> <max> <default> ["desc"]
            //   [APPEND_]ITEM         <name> [bit_offset] <bit_size> <type> ["desc"]
            //   ID variants insert an <id_value> (in the default column for
            //   parameters; right after the type for items).
            //   ARRAY variants add an <array_bit_size> right after the type.
            int idx = 1;
            if (idx < toks.size()) item.name = toks[idx++].toUpper();

            if (!isAppend && idx < toks.size()) {
                item.bitOffset = toks[idx].toInt();
                ++idx;
            }

            bool    bitOk  = true;
            QString bitTok;
            if (idx < toks.size()) {
                bitTok      = toks[idx];
                item.bitSize = toks[idx].toInt(&bitOk);
                ++idx;
            }

            if (idx < toks.size()) item.dataType = toks[idx++].toUpper();

            if (item.isArray && idx < toks.size()) {
                item.arrayBitSize = toks[idx].toInt();
                ++idx;
            }

            if (isParam) {
                const bool strBlock = (item.dataType == "STRING" || item.dataType == "BLOCK");
                if (!strBlock) {
                    if (idx < toks.size()) item.minVal = toks[idx++];
                    if (idx < toks.size()) item.maxVal = toks[idx++];
                }
                // default value, or id_value for ID parameters
                if (idx < toks.size()) item.defaultVal = toks[idx++];
            } else if (item.isId && !isParam) {
                // Reuse defaultVal for the id_value, same as ID parameters
                // above - this was previously just skipped (++idx) and
                // never captured anywhere, so PluginView's structure table
                // (which reads item.defaultVal into its DEFAULT column) had
                // no way to display or preserve an existing ID item's value,
                // silently dropping it from the file on any re-save.
                if (idx < toks.size()) item.defaultVal = toks[idx++];
            }

            if (idx < toks.size()) item.description = toks[idx];

            if (!item.dataType.isEmpty() && !isValidDataType(item.dataType)) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("Unknown data type '%1' for '%2'")
                        .arg(item.dataType).arg(item.name),
                    scopeOf(currentBlock->kind)
                });
            }

            // ── Bit size sanity ───────────────────────────────────────────────
            if (!bitTok.isEmpty()) {
                if (!bitOk) {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Error, lineNo,
                        QString("Bit size '%1' for '%2' is not an integer")
                            .arg(bitTok).arg(item.name),
                        scopeOf(currentBlock->kind)
                    });
                } else if (item.dataType.startsWith("FLOAT")) {
                    if (item.bitSize != 32 && item.bitSize != 64) {
                        result.diagnostics.append({
                            CmdTlmDiagnostic::Severity::Error, lineNo,
                            QString("FLOAT '%1' must be 32 or 64 bits, not %2")
                                .arg(item.name).arg(item.bitSize),
                            scopeOf(currentBlock->kind)
                        });
                    }
                } else if (item.bitSize <= 0 && item.dataType != "DERIVED") {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Error, lineNo,
                        QString("Bit size for '%1' must be positive").arg(item.name),
                        scopeOf(currentBlock->kind)
                    });
                }
            }

            // ── Numeric range sanity (non-ID parameters) ──────────────────────
            if (isParam) {
                bool         minOk = false;
                bool         maxOk = false;
                const double mn    = item.minVal.toDouble(&minOk);
                const double mx    = item.maxVal.toDouble(&maxOk);

                if (minOk && maxOk && mn > mx) {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Error, lineNo,
                        QString("Minimum (%1) is greater than maximum (%2) for '%3'")
                            .arg(item.minVal).arg(item.maxVal).arg(item.name),
                        scopeOf(currentBlock->kind)
                    });
                }

                if (!item.isId && !item.defaultVal.isEmpty()) {
                    bool         defOk = false;
                    const double dv    = item.defaultVal.toDouble(&defOk);
                    if (defOk && minOk && maxOk && (dv < mn || dv > mx)) {
                        result.diagnostics.append({
                            CmdTlmDiagnostic::Severity::Warning, lineNo,
                            QString("Default (%1) is outside the range [%2, %3] for '%4'")
                                .arg(item.defaultVal).arg(item.minVal)
                                .arg(item.maxVal).arg(item.name),
                            scopeOf(currentBlock->kind)
                        });
                    }
                }
            }

            currentBlock->items.append(item);
            continue;
        }

        // ── Item modifiers with checkable argument shapes ────────────────────
        const CmdTlmDiagnostic::Scope modScope =
            currentBlock ? scopeOf(currentBlock->kind) : CmdTlmDiagnostic::Scope::Any;

        if (kw == "STATE") {
            // STATE <key> <value> [color|HAZARDOUS ...]
            if (toks.size() < 3)
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Warning, lineNo,
                    QString("STATE requires a name and a value"),
                    modScope
                });
            continue;
        }
        if (kw == "UNITS") {
            // UNITS <full_name> <abbreviation>
            if (toks.size() < 3)
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Warning, lineNo,
                    QString("UNITS requires a full name and an abbreviation"),
                    modScope
                });
            continue;
        }
        if (kw == "LIMITS") {
            // LIMITS <set> <persistence> <enabled> <rl> <yl> <yh> <rh> [gl gh]
            const int n = static_cast<int>(toks.size()) - 1;
            if (n != 7 && n != 9)
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("LIMITS expects 7 or 9 values but got %1").arg(n),
                    modScope
                });
            continue;
        }

        // ── Known sub-directives — silently skip ─────────────────────────────
        if (kSubKeywords.contains(kw))
            continue;

        // ── Unknown keyword ───────────────────────────────────────────────────
        result.diagnostics.append({
            CmdTlmDiagnostic::Severity::Warning, lineNo,
            QString("Unknown keyword '%1'").arg(toks[0])
        });
    }

    // ── Post-parse: duplicate item name check ─────────────────────────────────
    for (const auto& block : result.blocks) {
        QSet<QString> seen;
        for (const auto& item : block.items) {
            if (seen.contains(item.name)) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, item.lineNumber,
                    QString("Duplicate item '%1' in %2 %3::%4")
                        .arg(item.name)
                        .arg(block.kind == CmdTlmBlock::Kind::Command ? "COMMAND" : "TELEMETRY")
                        .arg(block.targetName).arg(block.name),
                    scopeOf(block.kind)
                });
            }
            seen.insert(item.name);
        }
    }

    // ── Post-parse: blocks with no items ──────────────────────────────────────
    for (const auto& block : result.blocks) {
        if (block.items.isEmpty() && !block.isSelect
            && !block.targetName.isEmpty() && !block.name.isEmpty()) {
            result.diagnostics.append({
                CmdTlmDiagnostic::Severity::Warning, block.lineNumber,
                QString("%1 %2::%3 has no items defined")
                    .arg(block.kind == CmdTlmBlock::Kind::Command ? "COMMAND" : "TELEMETRY")
                    .arg(block.targetName).arg(block.name),
                scopeOf(block.kind)
            });
        }
    }

    return result;
}

} // namespace OpenC3::ViewModels
