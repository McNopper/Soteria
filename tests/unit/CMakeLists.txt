file(GLOB UNIT_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(soteria-unit-tests ${UNIT_SOURCES})

target_include_directories(soteria-unit-tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}          # project root → #include "engine/core/result.hpp"
)

target_link_libraries(soteria-unit-tests
    PRIVATE
        soteria-core
        GTest::gtest_main
)

# Inherit the project-wide warning flags from the parent scope but suppress
# GTest macro warnings that are unavoidable in test code.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(soteria-unit-tests PRIVATE /w44100)
else()
    target_compile_options(soteria-unit-tests PRIVATE
        -Wno-global-constructors   # GTest uses global ctors for test registration
    )
endif()

include(GoogleTest)
gtest_discover_tests(soteria-unit-tests
    TEST_PREFIX "unit."
    PROPERTIES LABELS "unit"
)
