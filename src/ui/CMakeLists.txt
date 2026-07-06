# ── UI Library ────────────────────────────────────────────────────────────────
# Qt Widgets layer. No business logic; binds to ViewModels only.

add_library(opencosmos_ui STATIC
    MainWindow.cpp
    widgets/StatusBadge.cpp
    widgets/LogWidget.cpp
    widgets/MetricCard.cpp
    widgets/CmdTlmHighlighter.cpp
    widgets/ScreenPreviewWidget.cpp
    views/DashboardView.cpp
    views/DockerView.cpp
    views/DoctorView.cpp
    views/PluginView.cpp
    views/SettingsView.cpp
    views/PacketToolsView.cpp
    views/ValidatorView.cpp
    views/InfraView.cpp
    views/LogViewerView.cpp
    dialogs/ConnectionDialog.cpp
    dialogs/AboutDialog.cpp
    dialogs/UserGuideDialog.cpp
    dialogs/PluginWizard.cpp
    dialogs/AddTargetDialog.cpp
    dialogs/NewScriptDialog.cpp
    dialogs/CmdTlmFieldDialog.cpp
    dialogs/PluginManifestModifierDialog.cpp
    dialogs/PluginManifestInterfaceDialog.cpp
    dialogs/NewMicroserviceDialog.cpp
    dialogs/ScreenWidgetDialog.cpp
    styles/ThemeManager.cpp
)

target_include_directories(opencosmos_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(opencosmos_ui
    PUBLIC
        opencosmos_viewmodels
        Qt6::Widgets
        Qt6::Network
)

target_set_warnings(opencosmos_ui)
enable_static_analysis(opencosmos_ui)
