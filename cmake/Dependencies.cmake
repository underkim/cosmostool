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
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# GoogleTest — only populated when testing is enabled
if(BUILD_TESTING)
    set(BUILD_GMOCK ON  CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
endif()

# libssh2 — WinCNG is the native Windows crypto backend (no external deps)
set(CRYPTO_BACKEND        "WinCNG" CACHE STRING "" FORCE)
set(BUILD_EXAMPLES        OFF CACHE BOOL "" FORCE)
set(LIBSSH2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libssh2)
