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
    "IGNORE_OVERLAP", "CCSDS_VER"
};

// ── Tokenizer (handles quoted strings) ───────────────────────────────────────

QStringList CmdTlmParser::tokenize(const QString& line)
{
    QStringList tokens;
    bool    inQuote = false;
    QString current;

    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (inQuote) {
                tokens.append(current);
                current.clear();
                inQuote = false;
            } else {
                if (!current.isEmpty()) {
                    tokens.append(current);
                    current.clear();
                }
                inQuote = true;
            }
        } else if ((c == ' ' || c == '\t') && !inQuote) {
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
                        QString("Unrecognised endianness '%1'").arg(block.endianness)
                    });
                }
            }

            if (block.targetName.isEmpty() || block.name.isEmpty()) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("%1 is missing target or packet name").arg(kw)
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
            item.isId       = kw.contains("ID_");

            const bool isParam  = kw.contains("PARAMETER");
            const bool isAppend = kw.startsWith("APPEND");
            const bool isArray  = kw.contains("ARRAY");

            // COSMOS grammar (positional). APPEND_* omit the leading bit_offset:
            //   [APPEND_]PARAMETER    <name> [bit_offset] <bit_size> <type> <min> <max> <default> ["desc"]
            //   [APPEND_]ITEM         <name> [bit_offset] <bit_size> <type> ["desc"]
            //   ID variants insert an <id_value> (in the default column for
            //   parameters; right after the type for items).
            //   ARRAY variants add an <array_bit_size> right after the type.
            int idx = 1;
            if (idx < toks.size()) item.name = toks[idx++].toUpper();

            if (!isAppend && idx < toks.size()) ++idx;   // skip bit_offset

            bool    bitOk  = true;
            QString bitTok;
            if (idx < toks.size()) {
                bitTok      = toks[idx];
                item.bitSize = toks[idx].toInt(&bitOk);
                ++idx;
            }

            if (idx < toks.size()) item.dataType = toks[idx++].toUpper();

            if (isArray && idx < toks.size()) ++idx;     // skip array_bit_size

            if (isParam && !isArray) {
                const bool strBlock = (item.dataType == "STRING" || item.dataType == "BLOCK");
                if (!strBlock) {
                    if (idx < toks.size()) item.minVal = toks[idx++];
                    if (idx < toks.size()) item.maxVal = toks[idx++];
                }
                // default value, or id_value for ID parameters
                if (idx < toks.size()) item.defaultVal = toks[idx++];
            } else if (item.isId && !isParam) {
                if (idx < toks.size()) ++idx;            // id_value (items)
            }

            if (idx < toks.size()) item.description = toks[idx];

            if (!item.dataType.isEmpty() && !isValidDataType(item.dataType)) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("Unknown data type '%1' for '%2'")
                        .arg(item.dataType).arg(item.name)
                });
            }

            // ── Bit size sanity ───────────────────────────────────────────────
            if (!bitTok.isEmpty()) {
                if (!bitOk) {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Error, lineNo,
                        QString("Bit size '%1' for '%2' is not an integer")
                            .arg(bitTok).arg(item.name)
                    });
                } else if (item.dataType.startsWith("FLOAT")) {
                    if (item.bitSize != 32 && item.bitSize != 64) {
                        result.diagnostics.append({
                            CmdTlmDiagnostic::Severity::Error, lineNo,
                            QString("FLOAT '%1' must be 32 or 64 bits, not %2")
                                .arg(item.name).arg(item.bitSize)
                        });
                    }
                } else if (item.bitSize <= 0 && item.dataType != "DERIVED") {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Error, lineNo,
                        QString("Bit size for '%1' must be positive").arg(item.name)
                    });
                }
            }

            // ── Numeric range sanity (non-ID parameters) ──────────────────────
            if (isParam && !isArray) {
                bool         minOk = false;
                bool         maxOk = false;
                const double mn    = item.minVal.toDouble(&minOk);
                const double mx    = item.maxVal.toDouble(&maxOk);

                if (minOk && maxOk && mn > mx) {
                    result.diagnostics.append({
                        CmdTlmDiagnostic::Severity::Error, lineNo,
                        QString("Minimum (%1) is greater than maximum (%2) for '%3'")
                            .arg(item.minVal).arg(item.maxVal).arg(item.name)
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
                                .arg(item.maxVal).arg(item.name)
                        });
                    }
                }
            }

            currentBlock->items.append(item);
            continue;
        }

        // ── Item modifiers with checkable argument shapes ────────────────────
        if (kw == "STATE") {
            // STATE <key> <value> [color|HAZARDOUS ...]
            if (toks.size() < 3)
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Warning, lineNo,
                    QString("STATE requires a name and a value")
                });
            continue;
        }
        if (kw == "UNITS") {
            // UNITS <full_name> <abbreviation>
            if (toks.size() < 3)
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Warning, lineNo,
                    QString("UNITS requires a full name and an abbreviation")
                });
            continue;
        }
        if (kw == "LIMITS") {
            // LIMITS <set> <persistence> <enabled> <rl> <yl> <yh> <rh> [gl gh]
            const int n = static_cast<int>(toks.size()) - 1;
            if (n != 7 && n != 9)
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("LIMITS expects 7 or 9 values but got %1").arg(n)
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
                        .arg(block.targetName).arg(block.name)
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
                    .arg(block.targetName).arg(block.name)
            });
        }
    }

    return result;
}

} // namespace OpenC3::ViewModels
