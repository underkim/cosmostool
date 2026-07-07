# ── Test Suite ─────────────────────────────────────────────────────────────────

add_executable(opencosmos_tests
    unit/app/StartupPolicyTest.cpp
    unit/app/ServiceRegistryTest.cpp
    unit/core/ExecutorResultTest.cpp
    unit/core/EventBusTest.cpp
    unit/core/ExecutorProxyTest.cpp
    unit/core/ShellQuoteTest.cpp
    unit/services/ConnectionServiceTest.cpp
    unit/services/DockerServiceTest.cpp
    unit/services/SystemServiceTest.cpp
    unit/services/DoctorServiceTest.cpp
    unit/services/PluginServiceTest.cpp
    unit/services/SettingsServiceTest.cpp
    unit/viewmodels/CmdTlmParserTest.cpp
    unit/viewmodels/PluginTemplateEngineTest.cpp
    unit/viewmodels/TextTokenizerTest.cpp
    unit/viewmodels/DiagnosticTest.cpp
    unit/viewmodels/ScreenParserTest.cpp
    unit/viewmodels/ScreenLayoutParserTest.cpp
    unit/viewmodels/PluginConfigParserTest.cpp
    unit/viewmodels/PluginManifestParserTest.cpp
    unit/viewmodels/ConfigValidatorTest.cpp
    unit/viewmodels/CommandValidatorTest.cpp
    unit/viewmodels/TelemetryValidatorTest.cpp
    unit/viewmodels/ProtocolValidatorTest.cpp
    unit/viewmodels/InterfaceValidatorTest.cpp
    unit/viewmodels/TargetConfigValidatorTest.cpp
    unit/viewmodels/RuleValidatorRegistryTest.cpp
    unit/viewmodels/ValidatorViewModelTest.cpp
    unit/viewmodels/PacketSimulatorTest.cpp
    unit/widgets/CmdTlmHighlighterTest.cpp
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
        opencosmos_ui           # for CmdTlmHighlighter - QSyntaxHighlighter/QTextDocument
                                # only, no QApplication/QWidget instantiated by this suite
        GTest::gtest_main
        GTest::gmock
        Qt6::Test
)

target_set_warnings(opencosmos_tests)

gtest_discover_tests(opencosmos_tests
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
