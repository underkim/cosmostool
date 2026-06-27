#include "CmdTlmParser.h"

#include <QSet>
#include <QStringList>

namespace OpenC3::ViewModels {

// ── Constants ─────────────────────────────────────────────────────────────────

static const QSet<QString> kValidDataTypes = {
    "INT8", "INT16", "INT32", "INT64",
    "UINT8", "UINT16", "UINT32", "UINT64",
    "FLOAT32", "FLOAT64",
    "STRING", "BLOCK", "DERIVED"
};

static const QSet<QString> kItemKeywords = {
    "APPEND_PARAMETER",    "PARAMETER",
    "APPEND_ID_PARAMETER", "ID_PARAMETER",
    "APPEND_ITEM",         "ITEM",
    "APPEND_ID_ITEM",      "ID_ITEM"
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

        // ── COMMAND / TELEMETRY block header ─────────────────────────────────
        if (kw == "COMMAND" || kw == "TELEMETRY") {
            CmdTlmBlock block;
            block.kind       = (kw == "COMMAND") ? CmdTlmBlock::Kind::Command
                                                  : CmdTlmBlock::Kind::Telemetry;
            block.lineNumber = lineNo;

            if (toks.size() >= 2) block.targetName = toks[1].toUpper();
            if (toks.size() >= 3) block.name       = toks[2].toUpper();
            if (toks.size() >= 4) block.endianness = toks[3].toUpper();
            if (toks.size() >= 5) block.description= toks[4];

            if (block.targetName.isEmpty() || block.name.isEmpty()) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("%1 is missing target or name").arg(kw)
                });
            }

            if (!block.endianness.isEmpty()
                && block.endianness != "BIG_ENDIAN"
                && block.endianness != "LITTLE_ENDIAN") {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Warning, lineNo,
                    QString("Unrecognised endianness '%1'").arg(block.endianness)
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

            // APPEND_PARAMETER <name> <bit_size> <type> <min> <max> <default> "desc"
            // APPEND_ITEM      <name> <bit_size> <type> "desc"
            const bool isParam = kw.contains("PARAMETER");

            if (toks.size() >= 2) item.name     = toks[1].toUpper();
            if (toks.size() >= 3) item.bitSize  = toks[2].toInt();
            if (toks.size() >= 4) item.dataType = toks[3].toUpper();

            if (isParam) {
                if (toks.size() >= 5) item.minVal     = toks[4];
                if (toks.size() >= 6) item.maxVal     = toks[5];
                if (toks.size() >= 7) item.defaultVal = toks[6];
                if (toks.size() >= 8) item.description= toks[7];
            } else {
                if (toks.size() >= 5) item.description = toks[4];
            }

            if (!item.dataType.isEmpty() && !isValidDataType(item.dataType)) {
                result.diagnostics.append({
                    CmdTlmDiagnostic::Severity::Error, lineNo,
                    QString("Unknown data type '%1' for '%2'")
                        .arg(item.dataType).arg(item.name)
                });
            }

            currentBlock->items.append(item);
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

    return result;
}

} // namespace OpenC3::ViewModels
