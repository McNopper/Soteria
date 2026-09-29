cmake_minimum_required(VERSION 3.22)

project(soteria
    VERSION 0.1.0
    DESCRIPTION "Soteria — Vulkan SC safety-critical rendering engine"
    LANGUAGES CXX
)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Export compile_commands.json for static-analysis tooling ──────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── cppcheck static-analysis targets (second analyzer beside clang-tidy) ──────
include(cmake/cppcheck.cmake)

# ── Locate Vulkan SC SDK ───────────────────────────────────────────────────────
# Set VULKANSC_SDK environment variable or pass -DCMAKE_PREFIX_PATH to cmake.
find_package(VulkanSC REQUIRED)

# ── Global compiler flags (Clang-cl / Clang) ──────────────────────────────────
# clang-cl uses MSVC-style /EH and /GR flags.
# /WX (-Werror) = treat all warnings as errors (mandatory for automotive/avionics).
# -Wswitch-enum enforces exhaustive enumerator handling in switch statements
# even when a default label is present (MISRA C++:2023 Rule 9.4.2 — a default
# label is required AND every enumerator must be covered).
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options(
        /EHs-c-                       # no C++ or SEH exceptions
        /GR-                          # no RTTI
        /W4                           # high warning level
        /WX                           # warnings are errors
        -Wshadow                      # inner-scope hiding (Rule 6.4.1)
        -Wconversion                  # implicit narrowing conversions
        -Wsign-conversion             # signed/unsigned conversions
        -Wnull-dereference
        -Wswitch-enum                 # exhaustive switch (Rule 9.4.2)
        # C++98/14 compat noise is irrelevant — we target C++17 exclusively
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-pre-c++17-compat
        -Wno-pre-c++14-compat
        # Vulkan C structs require explicit field writes; clang's buffer-safety
        # analysis flags fixed-size struct member access as potentially unsafe.
        # All engine buffers are std::array with explicit bounds.
        -Wno-unsafe-buffer-usage
    )
else()
    add_compile_options(
        -fno-exceptions
        -fno-rtti
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wswitch-enum
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-unsafe-buffer-usage
    )
endif()

# ── Subprojects ───────────────────────────────────────────────────────────────
# ── Logging — enabled in Debug only; compiled out in Release/production ───────
# VKSC_ENABLE_LOGGING is the single gate: when absent every log::Info/Warn/Error
# call compiles to an empty inline, and the *Impl bodies are stripped by the
# linker.  No log text or timing code reaches the production binary.
add_compile_definitions($<$<CONFIG:Debug>:VKSC_ENABLE_LOGGING>)

# ── Runtime DLL staging ───────────────────────────────────────────────────────
# Executables that link vulkansc-1.lib need the loader DLL next to them; the
# emulation ICD (vksconvk.dll) is required by device-dependent tests.  Stage
# both beside each target so it runs from its own directory without PATH
# tweaks (also lets gtest_discover_tests execute the binary at build time).
function(soteria_stage_runtime_dlls target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:VulkanSC::Loader>"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying vulkansc-1.dll to ${target} output directory"
    )
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VulkanSC_INCLUDE_DIR}/../bin/vksconvk.dll"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying vksconvk.dll (emulation layer) to ${target} output directory"
    )
endfunction()

add_subdirectory(engine)
add_subdirectory(app)

# ── Tests ─────────────────────────────────────────────────────────────────────
# Requires: VulkanSC SDK (for soteria-core) + internet access for GTest fetch.
# Disable with -DSOTERIA_BUILD_TESTS=OFF when the SDK is not available.
option(SOTERIA_BUILD_TESTS "Build and register unit, integration and qualification tests" ON)
if(SOTERIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Analysis lanes as ctest entries ───────────────────────────────────────────
# Run static analysis through the same `ctest` flow as the runtime tests.
# Each lane exits 77 (SKIP_RETURN_CODE) when its toolchain or the compile
# database is unavailable, so partial environments degrade gracefully.
#   ctest -L analysis --output-on-failure
find_program(PYTHON_EXE NAMES python python3)
if(PYTHON_EXE AND SOTERIA_BUILD_TESTS)
    add_test(NAME test_layers
             COMMAND ${PYTHON_EXE} ${CMAKE_SOURCE_DIR}/tools/check_layers.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_layers PROPERTIES
        LABELS analysis
        SKIP_RETURN_CODE 77)

    add_test(NAME test_tidy
             COMMAND ${PYTHON_EXE} ${CMAKE_SOURCE_DIR}/tools/check_tidy.py
                     --db ${CMAKE_BINARY_DIR}/compile_commands.json
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(test_tidy PROPERTIES
        LABELS analysis
        SKIP_RETURN_CODE 77)
endif()
