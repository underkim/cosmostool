#pragma once

#include "ui/AppMode.h"

namespace OpenC3::App {

/// Whether Application::run() should show the blocking startup
/// ConnectionDialog before MainWindow is constructed. Extracted as a pure
/// function so the mode x profile-existence decision is unit-testable
/// without spinning up QApplication or the full DI-wired Application.
///
/// Plugin Creation mode never blocks on this dialog - MainWindow's own
/// silent autoConnectIfNeeded() (default/first saved profile, or a
/// headlessly-created Quick-WSL profile if none exists yet) handles
/// connecting regardless of whether any profile is saved. Connect & Operate
/// mode keeps the existing behavior (always show the dialog), since that
/// mode expects an explicit, user-confirmed connection.
[[nodiscard]] inline bool shouldShowStartupConnectionDialog(
    UI::AppMode mode, bool /*hasAnyProfile*/) noexcept
{
    return mode != UI::AppMode::PluginCreation;
}

} // namespace OpenC3::App
