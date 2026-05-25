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

# ── Locate Vulkan SC SDK ───────────────────────────────────────────────────────
# Set VULKANSC_SDK environment variable or pass -DCMAKE_PREFIX_PATH to cmake.
find_package(VulkanSC REQUIRED)

# ── Global compiler flags (Clang-cl / Clang) ──────────────────────────────────
# clang-cl uses MSVC-style /EH and /GR flags.
# /WX = treat all warnings as errors (mandatory for automotive/avionics).
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options(
        /EHs-c-                       # no C++ or SEH exceptions
        /GR-                          # no RTTI
        /W4                           # high warning level
        /WX                           # warnings are errors
        # C++98/14 compat noise is irrelevant — we target C++17 exclusively
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-pre-c++17-compat
        -Wno-pre-c++14-compat
        # Array subscript on stack-allocated fixed arrays with explicit bounds
        # checks is safe; std::span requires STL which is excluded.
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
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wno-unsafe-buffer-usage
    )
endif()

# ── Subprojects ───────────────────────────────────────────────────────────────
# ── Logging — enabled in Debug only; compiled out in Release/production ───────
# VKSC_ENABLE_LOGGING is the single gate: when absent every log::Info/Warn/Error
# call compiles to an empty inline, and the *Impl bodies are stripped by the
# linker.  No log text or timing code reaches the production binary.
add_compile_definitions($<$<CONFIG:Debug>:VKSC_ENABLE_LOGGING>)

add_subdirectory(engine)
add_subdirectory(simulation)
