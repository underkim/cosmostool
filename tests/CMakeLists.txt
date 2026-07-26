# ── Test Suite ─────────────────────────────────────────────────────────────────

add_executable(opencosmos_tests
    unit/app/StartupPolicyTest.cpp
    unit/app/ServiceRegistryTest.cpp
    unit/core/ExecutorResultTest.cpp
    unit/core/EventBusTest.cpp
    unit/core/ExecutorProxyTest.cpp
    unit/core/ShellQuoteTest.cpp
    unit/core/ApiClientProxyTest.cpp
    unit/core/ConnectionProfileApiTest.cpp
    unit/services/ConnectionServiceTest.cpp
    unit/services/DockerServiceTest.cpp
    unit/services/SystemServiceTest.cpp
    unit/services/DoctorServiceTest.cpp
    unit/services/CosmosPluginServiceTest.cpp
    unit/services/CosmosScriptServiceTest.cpp
    unit/services/CosmosRuntimeServiceTest.cpp
    unit/services/CosmosCommandServiceTest.cpp
    unit/services/WorkspaceServiceTest.cpp
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
    unit/viewmodels/PluginViewModelTest.cpp
    unit/viewmodels/ScriptViewModelTest.cpp
    unit/viewmodels/RuntimeViewModelTest.cpp
    unit/viewmodels/CommandSenderViewModelTest.cpp
    unit/viewmodels/DoctorViewModelTest.cpp
    unit/viewmodels/PacketToolsViewModelTest.cpp
    unit/viewmodels/InfraViewModelTest.cpp
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

# ── Live transport check (manual) ─────────────────────────────────────────────
# Excluded from the gtest suite and from ALL: it binds real localhost sockets to
# exercise HttpCosmosApiClient's actual Qt Network path, which the mock-based
# unit tests cannot cover. Build and run it explicitly:
#   cmake --build <dir> --target opencosmos_api_livecheck
#   <dir>/bin/opencosmos_api_livecheck
add_executable(opencosmos_api_livecheck EXCLUDE_FROM_ALL
    manual/HttpCosmosApiClientLiveCheck.cpp
)

target_include_directories(opencosmos_api_livecheck
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(opencosmos_api_livecheck
    PRIVATE
        opencosmos_viewmodels
        Qt6::Core
        Qt6::Network
)

# ── Real COSMOS integration check (manual) ───────────────────────────────────
# Unlike opencosmos_api_livecheck's local capture server, this executable talks
# to a running COSMOS 6.10.4 stack through the production HttpCosmosApiClient
# and service implementations. It is EXCLUDE_FROM_ALL and is never registered
# with CTest because it requires an explicitly supplied live credential and
# environment. Its default mode performs reads plus one deliberately
# nonexistent command (error-path only) and never sends a valid command.
# OPENC3_LIVE_MUTATING=1 additionally installs/runs/uninstalls only a reserved
# temporary live-check plugin and verifies cleanup.
#   cmake --build <dir> --target opencosmos_live_cosmos_check
#   OPENC3_LIVE_PASSWORD=... <dir>/bin/opencosmos_live_cosmos_check [url] [scope]
add_executable(opencosmos_live_cosmos_check EXCLUDE_FROM_ALL
    manual/LiveCosmosIntegrationCheck.cpp
)

target_include_directories(opencosmos_live_cosmos_check
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(opencosmos_live_cosmos_check
    PRIVATE
        opencosmos_services
        opencosmos_viewmodels
        Qt6::Core
        Qt6::Network
)

# ── Startup wiring check (manual) ─────────────────────────────────────────────
# The shipped app is WIN32_EXECUTABLE, so its console log sink goes nowhere and
# its file sink only flushes at warn level - a GUI run killed by a timeout
# leaves no evidence that startup actually succeeded. This console executable
# builds the same composition root and reports the result on stdout.
#   cmake --build <dir> --target opencosmos_startup_check
#   <dir>/bin/opencosmos_startup_check
add_executable(opencosmos_startup_check EXCLUDE_FROM_ALL
    manual/StartupWiringCheck.cpp
)

target_include_directories(opencosmos_startup_check
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(opencosmos_startup_check
    PRIVATE
        opencosmos_viewmodels
        Qt6::Core
        Qt6::Network
)

# gtest_discover_tests() runs the freshly-linked test binary at CMake-generate
# time to enumerate individual TEST()s, writing its scratch file next to the
# build tree. On a non-ASCII (e.g. Korean) Windows path that scratch file is
# written under a mis-encoded path the binary itself then can't reopen,
# turning every otherwise-successful build into a spurious post-link failure
# ("Unable to open file ... cmake_test_discovery_*.json"). gtest_add_tests()
# gets the same per-TEST() ctest granularity by parsing the *source files* at
# configure time instead of executing anything, so it isn't sensitive to the
# runtime working directory's encoding. Keep gtest_discover_tests() on
# non-Windows, where CI and local encoding are ASCII and the run-time
# discovery has no such failure mode.
if(WIN32)
    gtest_add_tests(
        TARGET opencosmos_tests
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
else()
    gtest_discover_tests(opencosmos_tests
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()
