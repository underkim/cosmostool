#pragma once

#include <QString>

// ── plugin.txt manifest block starter snippets ───────────────────────────────
// Single source of truth for the "New Block" boilerplate the Manifest tab
// inserts (PluginView.cpp's onAddManifestBlockClicked-style flow) - TARGET is
// deliberately excluded here since it already has its own dedicated creation
// flow (AddTargetDialog, wired to "Add Target"), which also scaffolds the
// target's folder structure, not just a plugin.txt line. INTERFACE/ROUTER and
// MICROSERVICE are also excluded - unlike these plain-text snippets, they're
// beginner-unfriendly enough (connection type/host/port/class file
// conventions for Interface/Router; folder/name/cmd for Microservice) to
// warrant a guided dialog instead (PluginManifestInterfaceDialog,
// NewMicroserviceDialog), not a raw template.

namespace OpenC3::UI::Widgets::PluginManifestSnippets {

inline const QString& toolBlock()
{
    static const QString s =
        "\nTOOL new_tool\n"
        "  URL /tools/new_tool\n";
    return s;
}

inline const QString& widgetBlock()
{
    static const QString s =
        "\nWIDGET new_widget\n"
        "  URL /widgets/new_widget\n";
    return s;
}

inline const QString& variableBlock()
{
    static const QString s = "\nVARIABLE new_variable default_value\n";
    return s;
}

} // namespace OpenC3::UI::Widgets::PluginManifestSnippets
