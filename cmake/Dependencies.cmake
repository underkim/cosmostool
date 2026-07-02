# ── Third-Party Dependencies via FetchContent ─────────────────────────────────
include(FetchContent)

# spdlog — fast header-only (and compiled) logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# nlohmann/json — modern JSON for C++
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# GoogleTest — unit and integration testing
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# libssh2 — SSH client library
# On Windows, requires OpenSSL or WinCNG backend.
# If vcpkg is available, prefer: vcpkg install libssh2
# Otherwise FetchContent builds from source.
FetchContent_Declare(
    libssh2
    GIT_REPOSITORY https://github.com/libssh2/libssh2.git
    GIT_TAG        libssh2-1.11.0
    GIT_SHALLOW    TRUE
)

# ── Populate ──────────────────────────────────────────────────────────────────
# Prefer system / pre-installed packages when present (Linux distros, CI images,
# vcpkg) so a local build does not require network access. Fall back to building
# the pinned source via FetchContent otherwise (the default path on Windows).

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# GoogleTest — only populated when testing is enabled
if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        set(BUILD_GMOCK ON  CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    include(GoogleTest)
endif()

# libssh2 — the core library links a target named `libssh2_static`. Use a system
# package when available (CMake config or pkg-config); otherwise build the
# pinned source with FetchContent.
find_package(Libssh2 QUIET)
if(TARGET Libssh2::libssh2)
    if(NOT TARGET libssh2_static)
        add_library(libssh2_static INTERFACE IMPORTED GLOBAL)
        target_link_libraries(libssh2_static INTERFACE Libssh2::libssh2)
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSSH2 QUIET IMPORTED_TARGET libssh2)
    endif()
    if(TARGET PkgConfig::LIBSSH2)
        if(NOT TARGET libssh2_static)
            add_library(libssh2_static INTERFACE IMPORTED GLOBAL)
            target_link_libraries(libssh2_static INTERFACE PkgConfig::LIBSSH2)
        endif()
    else()
        # ── Build libssh2 from source ──────────────────────────────────────────
        #   Windows: WinCNG is native and needs no external dependencies.
        #   Other:   OpenSSL (provide libssl-dev / OpenSSL on the build machine).
        if(WIN32)
            set(CRYPTO_BACKEND    "WinCNG"  CACHE STRING "" FORCE)
        else()
            set(CRYPTO_BACKEND    "OpenSSL" CACHE STRING "" FORCE)
        endif()
        set(BUILD_EXAMPLES        OFF CACHE BOOL "" FORCE)
        set(LIBSSH2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)

        # libssh2 1.11.0 declares cmake_minimum_required(VERSION 2.8.x). CMake
        # >= 4.0 rejects that outright, so permit the older policy version for
        # the bundled source to keep it configurable under modern toolchains.
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        FetchContent_MakeAvailable(libssh2)
    endif()
endif()
