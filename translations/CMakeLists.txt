# ── Translations ──────────────────────────────────────────────────────────────
# openc3_ko.ts is source-controlled and updated by hand (or via `lupdate` run
# manually during development, never as part of a normal build - it rewrites
# a tracked file and would otherwise silently drop translations whenever
# source strings shift). Only the .ts -> .qm compile step (lrelease) runs
# automatically here, producing the binary catalog the app loads at runtime.
#
# The .qm is placed directly in the runtime output directory (next to the
# executable), not embedded via resources.qrc - AUTORCC needs its file list
# to already exist at CMake configure time, which would fight a build-time
# lrelease step. A plain runtime-relative path is simpler in both the dev
# build and (once packaged) an installed build alike.

set(OPENC3_TS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/openc3_ko.ts")
set(OPENC3_QM_DIR   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations")
set(OPENC3_QM_FILE  "${OPENC3_QM_DIR}/openc3_ko.qm")

if(LRELEASE_EXECUTABLE)
    add_custom_command(
        OUTPUT "${OPENC3_QM_FILE}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENC3_QM_DIR}"
        COMMAND "${LRELEASE_EXECUTABLE}" -silent "${OPENC3_TS_FILE}" -qm "${OPENC3_QM_FILE}"
        DEPENDS "${OPENC3_TS_FILE}"
        COMMENT "Compiling Korean translation (lrelease)"
        VERBATIM
    )
    add_custom_target(openc3_translations ALL DEPENDS "${OPENC3_QM_FILE}")
else()
    message(WARNING "lrelease not found - Korean translation (.qm) will not be built. "
                     "Install Qt's Linguist tools (e.g. the qt6-l10n-tools package) to enable it.")
    add_custom_target(openc3_translations)
endif()
