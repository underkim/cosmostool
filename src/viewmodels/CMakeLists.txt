# ── ViewModel Library ─────────────────────────────────────────────────────────
# MVVM bridge layer. First layer to depend on Qt.
# ViewModels expose Q_PROPERTY / signals to the UI and call pure-C++ services.

add_library(opencosmos_viewmodels STATIC
    base/ViewModelBase.cpp
    dashboard/DashboardViewModel.cpp
    docker/DockerViewModel.cpp
    doctor/DoctorViewModel.cpp
    settings/SettingsViewModel.cpp
    plugin/PluginViewModel.cpp
    cmdtlm/CmdTlmViewModel.cpp
    packettools/PacketToolsViewModel.cpp
    logviewer/LogViewerViewModel.cpp
    infra/InfraViewModel.cpp
)

target_include_directories(opencosmos_viewmodels
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(opencosmos_viewmodels
    PUBLIC
        opencosmos_services
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
)

target_set_warnings(opencosmos_viewmodels)
enable_static_analysis(opencosmos_viewmodels)
