# ── src/CMakeLists.txt ────────────────────────────────────────────────────────
# Coordinates layer targets. Dependency arrows:
#
#   [core] ← [services] ← [viewmodels] ← [ui + app] → executable
#
# Only viewmodels and above link against Qt.

add_subdirectory(core)
add_subdirectory(models)      # header-only; core depends on it
add_subdirectory(services)
add_subdirectory(viewmodels)
add_subdirectory(ui)
add_subdirectory(app)
