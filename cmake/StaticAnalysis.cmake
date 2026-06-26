# ── Static Analysis Integration ───────────────────────────────────────────────
# Opt-in; enable via -DENABLE_CLANG_TIDY=ON or -DENABLE_CPPCHECK=ON

option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(ENABLE_CPPCHECK   "Run cppcheck during compilation"   OFF)

function(enable_static_analysis target)
    if(ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES "clang-tidy" "clang-tidy-17" "clang-tidy-16")
        if(CLANG_TIDY_EXE)
            set_target_properties(${target} PROPERTIES
                CXX_CLANG_TIDY
                    "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
            )
            message(STATUS "[StaticAnalysis] clang-tidy enabled for ${target}")
        else()
            message(WARNING "[StaticAnalysis] clang-tidy requested but not found")
        endif()
    endif()

    if(ENABLE_CPPCHECK)
        find_program(CPPCHECK_EXE NAMES "cppcheck")
        if(CPPCHECK_EXE)
            set_target_properties(${target} PROPERTIES
                CXX_CPPCHECK
                    "${CPPCHECK_EXE};--enable=warning,style,performance,portability;--error-exitcode=1"
            )
            message(STATUS "[StaticAnalysis] cppcheck enabled for ${target}")
        else()
            message(WARNING "[StaticAnalysis] cppcheck requested but not found")
        endif()
    endif()
endfunction()
