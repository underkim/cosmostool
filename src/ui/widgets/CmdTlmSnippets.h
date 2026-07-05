#pragma once

#include <QString>

// ── CMD/TLM editing snippets & helpers ───────────────────────────────────────
// Single source of truth for the COSMOS CMD/TLM boilerplate that the Workspace
// view's (PluginView) embedded component editor inserts, plus the shared
// file-kind check.

namespace OpenC3::UI::Widgets::CmdTlmSnippets {

// Canonical COMMAND block template (insert-at-cursor).
inline const QString& command()
{
    static const QString s =
        "COMMAND TARGET_NAME COMMAND_NAME BIG_ENDIAN \"Command description\"\n"
        "  APPEND_ID_PARAMETER CMD_ID 16 UINT 1 1 1 \"Command ID\"\n"
        "  APPEND_PARAMETER PARAM1 32 FLOAT 0 100 0.0 \"Parameter description\"\n";
    return s;
}

// Canonical TELEMETRY block template.
inline const QString& telemetry()
{
    static const QString s =
        "TELEMETRY TARGET_NAME PACKET_NAME BIG_ENDIAN \"Telemetry description\"\n"
        "  APPEND_ITEM SYNC_WORD 32 UINT \"Sync pattern\"\n"
        "  APPEND_ITEM ITEM1 32 FLOAT \"Item description\"\n";
    return s;
}

// Single APPEND_PARAMETER line template.
inline const QString& parameter()
{
    static const QString s =
        "  APPEND_PARAMETER PARAM_NAME 32 FLOAT 0.0 100.0 0.0 \"Parameter description\"\n";
    return s;
}

// True when the path lives under a COSMOS cmd_tlm directory (the files that
// carry COMMAND/TELEMETRY definitions).
inline bool isCmdTlmFile(const QString& path)
{
    return path.contains(QStringLiteral("/cmd_tlm/"), Qt::CaseInsensitive)
        || path.contains(QStringLiteral("\\cmd_tlm\\"), Qt::CaseInsensitive);
}

} // namespace OpenC3::UI::Widgets::CmdTlmSnippets
