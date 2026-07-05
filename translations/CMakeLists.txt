# ── Translations ──────────────────────────────────────────────────────────────
# openc3_ko.ts is source-controlled and updated by hand (or via `lupdate` run
# manually during development, never as part of a normal build - it rewrites
# a tracked file and would otherwise silently drop translations whenever
# source strings shift). Only the .ts -> .qm compile step (lrelease) runs
# automatically here, producing the binary catalog the app loads at runtime.
#
# The .qm used to be placed as a loose file next to the executable and
# loaded via applicationDirPath(). That broke for anyone who copies just the
# .exe (and windeployqt's DLLs) into a separate "portable" folder without
# also bringing the translations/ subdirectory along - the app would fall
# back to English with no visible error. Embedding the .qm into the binary
# via Qt's resource system instead means a single copied .exe always carries
# its own translation catalog. AUTORCC can't do this directly (its file list
# must exist at CMake configure time, which fights this build-time lrelease
# step), so this uses qt_add_resources() in src/app/CMakeLists.txt instead,
# which supports a build-generated input file.

set(OPENC3_TS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/openc3_ko.ts")
set(OPENC3_QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/openc3_ko.qm")

if(LRELEASE_EXECUTABLE)
    add_custom_command(
        OUTPUT "${OPENC3_QM_FILE}"
        COMMAND "${LRELEASE_EXECUTABLE}" -silent "${OPENC3_TS_FILE}" -qm "${OPENC3_QM_FILE}"
        DEPENDS "${OPENC3_TS_FILE}"
        COMMENT "Compiling Korean translation (lrelease)"
        VERBATIM
    )
    add_custom_target(openc3_translations DEPENDS "${OPENC3_QM_FILE}")
else()
    message(WARNING "lrelease not found - Korean translation (.qm) will not be built. "
                     "Install Qt's Linguist tools (e.g. the qt6-l10n-tools package) to enable it.")
    add_custom_target(openc3_translations)
endif()

# Visible to src/app/CMakeLists.txt, which add_subdirectory(src) reaches
# after this directory - a plain set() wouldn't cross that subdirectory
# boundary.
set(OPENC3_QM_FILE "${OPENC3_QM_FILE}" CACHE INTERNAL "")
