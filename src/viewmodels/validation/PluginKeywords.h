#pragma once

#include <QSet>
#include <QString>

namespace OpenC3::ViewModels::Validation::PluginKeywords {

// Shared COSMOS plugin.txt keyword sets, used by both PluginConfigParser and
// InterfaceValidator so the recognised keyword lists live in exactly one place.

// Top-level declarations that open a context modifiers attach to.
[[nodiscard]] const QSet<QString>& topLevel();

// Modifiers valid under INTERFACE / ROUTER.
[[nodiscard]] const QSet<QString>& interfaceModifiers();

// Modifiers valid under MICROSERVICE.
[[nodiscard]] const QSet<QString>& microserviceModifiers();

// Modifiers valid under TARGET.
[[nodiscard]] const QSet<QString>& targetModifiers();

// Modifiers valid under TOOL / WIDGET.
[[nodiscard]] const QSet<QString>& toolModifiers();

} // namespace OpenC3::ViewModels::Validation::PluginKeywords
