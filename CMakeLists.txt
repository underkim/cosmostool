cmake_minimum_required(VERSION 3.25)

project(OpenC3DevToolkit
    VERSION 0.1.0
    DESCRIPTION "OpenC3 COSMOS Developer Toolkit"
    LANGUAGES CXX
)

# ── C++ Standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt Automation ─────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── CMake Modules ─────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Define BUILD_TESTING before Dependencies so GoogleTest — which provides
# gtest_discover_tests — is set up whenever the test suite is enabled.
option(BUILD_TESTING "Build test suite" ON)

include(CompilerWarnings)
include(Dependencies)
include(StaticAnalysis)

# ── Output Directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Multi-config generators (Visual Studio, Xcode) otherwise append a
# per-config subdirectory (e.g. bin/Debug/OpenC3DevToolkit.exe) on top of
# CMAKE_RUNTIME_OUTPUT_DIRECTORY, silently breaking anything that assumes a
# flat bin/ layout - notably translations/CMakeLists.txt, which places
# openc3_ko.qm directly under bin/translations/ and is looked up at runtime
# via applicationDirPath()/translations/openc3_ko.qm. Pin every config to
# the same flat directory so the .qm always lands next to the real exe.
if(CMAKE_CONFIGURATION_TYPES)
    foreach(_config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${_config}" _config_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    endforeach()
endif()

# ── Find Qt6 ──────────────────────────────────────────────────────────────────
# Set Qt6_DIR or CMAKE_PREFIX_PATH before configuring:
#   cmake -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64" ..
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Concurrent
    Test
)

qt_standard_project_setup()

# ── Install Qt runtime (Windows) ──────────────────────────────────────────────
if(WIN32)
    get_target_property(_QT_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_QT_BIN_DIR "${_QT_QMAKE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_QT_BIN_DIR}")
endif()

# ── Translations ──────────────────────────────────────────────────────────────
# Found directly via find_program() rather than find_package(Qt6 COMPONENTS
# LinguistTools) - on some Linux distros (Debian/Ubuntu's qt6-l10n-tools) the
# lupdate/lrelease binaries are packaged without the LinguistTools CMake
# package config, so relying on it would break there even though the official
# Qt SDK ships both together.
if(NOT WIN32)
    get_target_property(_QT_QMAKE_PATH Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_QT_BIN_DIR "${_QT_QMAKE_PATH}" DIRECTORY)
endif()
find_program(LRELEASE_EXECUTABLE NAMES lrelease lrelease-qt6 lrelease6
    HINTS "${_QT_BIN_DIR}" /usr/lib/qt6/bin)

add_subdirectory(translations)

# ── Subdirectories ────────────────────────────────────────────────────────────
add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
