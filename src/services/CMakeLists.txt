# ── Services Library ──────────────────────────────────────────────────────────
# Business logic. No Qt dependency. Depends on core + models.

add_library(opencosmos_services STATIC
    connection/ConnectionService.cpp
    docker/DockerService.cpp
    system/SystemService.cpp
    doctor/DoctorService.cpp
    plugin/PluginService.cpp
    settings/SettingsService.cpp
    filesystem/RemoteFileService.cpp
)

target_include_directories(opencosmos_services
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(opencosmos_services
    PUBLIC
        opencosmos_core
        opencosmos_models
)

target_set_warnings(opencosmos_services)
enable_static_analysis(opencosmos_services)
