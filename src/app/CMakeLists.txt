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
