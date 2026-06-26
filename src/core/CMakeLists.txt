# ── Core Library ──────────────────────────────────────────────────────────────
# No Qt dependency. Pure C++20.
# Depends on: spdlog, nlohmann_json, libssh2, models (header-only)

add_library(opencosmos_core STATIC
    logging/Logger.cpp
    events/EventBus.cpp
    connection/wsl/WslExecutor.cpp
    connection/ssh/SshExecutor.cpp
)

target_include_directories(opencosmos_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${libssh2_SOURCE_DIR}/include"
)

target_link_libraries(opencosmos_core
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        libssh2_static
)

if(WIN32)
    target_link_libraries(opencosmos_core PUBLIC ws2_32)
endif()

target_set_warnings(opencosmos_core)
enable_static_analysis(opencosmos_core)
