# ── Models (Header-Only Interface Library) ────────────────────────────────────
# Domain models have zero dependencies — they are plain C++ structs.
# Exposed as INTERFACE so consumers get the include path without linking.

add_library(opencosmos_models INTERFACE)

target_include_directories(opencosmos_models
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
