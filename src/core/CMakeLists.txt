# ── Core Library ──────────────────────────────────────────────────────────────
# No Qt dependency. Pure C++20.
# Depends on: spdlog, nlohmann_json, libssh2, models (header-only)

add_library(opencosmos_core STATIC
    logging/Logger.cpp
    events/EventBus.cpp
    connection/ExecutorProxy.cpp
    connection/ShellQuote.cpp
    connection/wsl/WslExecutor.cpp
    connection/ssh/SshExecutor.cpp
)

target_include_directories(opencosmos_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# libssh2 is third-party: expose its headers as SYSTEM so warnings from
# libssh2.h (e.g. old-style casts inside its macros) don't trip our -Werror.
# Only needed for the FetchContent build; a system package carries its own
# include dirs transitively on the libssh2_static target.
if(libssh2_SOURCE_DIR)
    target_include_directories(opencosmos_core SYSTEM PUBLIC
        "${libssh2_SOURCE_DIR}/include"
    )
endif()

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
