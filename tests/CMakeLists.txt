# ── Test Suite ─────────────────────────────────────────────────────────────────

add_executable(opencosmos_tests
    unit/core/ExecutorResultTest.cpp
    unit/core/EventBusTest.cpp
    unit/services/DockerServiceTest.cpp
    unit/services/DoctorServiceTest.cpp
    unit/services/SettingsServiceTest.cpp
)

target_include_directories(opencosmos_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(opencosmos_tests
    PRIVATE
        opencosmos_services
        GTest::gtest_main
        GTest::gmock
)

target_set_warnings(opencosmos_tests)

gtest_discover_tests(opencosmos_tests
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
