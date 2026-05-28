# ── Google Test via FetchContent ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
# Prevent GTest from overriding our /MT / /MD choice on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# We only need gtest, not gmock.
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Suppress warnings in GTest headers (they are not our code).
if(TARGET gtest)
    target_compile_options(gtest  PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-w>)
    target_compile_options(gtest  PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/w>)
    target_compile_options(gtest_main PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-w>)
    target_compile_options(gtest_main PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/w>)
endif()

add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(qualification)
