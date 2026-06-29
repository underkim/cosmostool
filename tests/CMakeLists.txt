# ── Test Suite ─────────────────────────────────────────────────────────────────

add_executable(opencosmos_tests
    unit/core/ExecutorResultTest.cpp
    unit/core/EventBusTest.cpp
    unit/core/ShellQuoteTest.cpp
    unit/services/DockerServiceTest.cpp
    unit/services/DoctorServiceTest.cpp
    unit/services/PluginServiceTest.cpp
    unit/services/SettingsServiceTest.cpp
    unit/viewmodels/CmdTlmParserTest.cpp
    unit/viewmodels/PluginTemplateEngineTest.cpp
    unit/viewmodels/ScreenParserTest.cpp
    unit/viewmodels/PluginConfigParserTest.cpp
    unit/viewmodels/ConfigValidatorTest.cpp
)

target_include_directories(opencosmos_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(opencosmos_tests
    PRIVATE
        opencosmos_services
        opencosmos_viewmodels   # for CmdTlmParser / PluginTemplateEngine (Qt-based)
        GTest::gtest_main
        GTest::gmock
)

target_set_warnings(opencosmos_tests)

gtest_discover_tests(opencosmos_tests
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
