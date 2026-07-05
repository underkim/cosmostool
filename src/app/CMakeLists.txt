# ── Application Entry Point ───────────────────────────────────────────────────
# Wires all layers together via the DI container, then launches Qt event loop.

qt_add_executable(OpenC3DevToolkit
    main.cpp
    Application.cpp
    ServiceRegistry.cpp
    "${CMAKE_SOURCE_DIR}/resources/resources.qrc"
)

target_include_directories(OpenC3DevToolkit
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(OpenC3DevToolkit
    PRIVATE
        opencosmos_ui
        Qt6::Widgets
)

# Embeds the build-generated openc3_ko.qm (see translations/CMakeLists.txt)
# straight into the executable, so a single copied .exe always carries its
# own Korean translation - no loose translations/ folder to leave behind
# when the app is copied elsewhere (e.g. a "portable" deployment).
get_filename_component(OPENC3_QM_DIR "${OPENC3_QM_FILE}" DIRECTORY)
set_source_files_properties("${OPENC3_QM_FILE}" PROPERTIES GENERATED TRUE)
qt_add_resources(OpenC3DevToolkit "openc3_translations_qrc"
    PREFIX "/translations"
    BASE "${OPENC3_QM_DIR}"
    FILES "${OPENC3_QM_FILE}"
)
add_dependencies(OpenC3DevToolkit openc3_translations)

set_target_properties(OpenC3DevToolkit PROPERTIES
    WIN32_EXECUTABLE TRUE   # No console window in release
    MACOSX_BUNDLE   TRUE
)

target_set_warnings(OpenC3DevToolkit)

# ── Windows Deploy ─────────────────────────────────────────────────────────────
if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET OpenC3DevToolkit POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --no-opengl-sw
            "$<TARGET_FILE:OpenC3DevToolkit>"
        COMMENT "Running windeployqt..."
    )
endif()
