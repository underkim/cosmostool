# ── Compiler Warning Configuration ────────────────────────────────────────────
# Centralises all warning flags so every target opts in via a helper function.

function(target_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            # /MP compiles this target's translation units in parallel (one
            # cl.exe worker per source file, up to the machine's core count).
            # This project's own targets form a strictly layered dependency
            # chain (app -> ui -> viewmodels -> services -> core), so MSBuild's
            # *project*-level --parallel/-m has little to parallelize across
            # targets; /MP is what actually parallelizes compiling the many
            # .cpp files *within* each target (e.g. opencosmos_ui, which alone
            # has 30+ source files) and is the real lever for build speed here.
            /MP
            /utf-8
            /W4
            /WX
            /permissive-
            /w14242  # 'id': conversion, possible loss of data
            /w14254  # operator: conversion, possible loss of data
            /w14263  # member function does not override base class virtual member function
            /w14265  # class has virtual functions, but destructor is not virtual
            /w14287  # unsigned/negative constant mismatch
            /we4289  # loop control variable used outside for-loop scope
            /w14296  # expression is always false/true
            /w14311  # pointer truncation
            /w14545  # expression before comma has no effect
            /w14546  # function call before comma missing argument list
            /w14547  # operator before comma has no effect
            /w14549  # operator before comma has no effect
            /w14555  # expression has no effect
            /w14619  # pragma warning: no warning number X
            /w14640  # thread unsafe static member initialization
            /w14826  # conversion is sign-extended
            /w14905  # wide string literal cast
            /w14906  # string literal cast
            /w14928  # illegal copy-initialization
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -Wno-gnu-zero-variadic-macro-arguments
            )
        endif()
    endif()
endfunction()
