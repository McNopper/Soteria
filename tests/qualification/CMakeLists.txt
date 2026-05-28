file(GLOB QUALIFICATION_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(soteria-qualification-tests ${QUALIFICATION_SOURCES})

target_include_directories(soteria-qualification-tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}          # project root → #include "engine/core/vksc_context.hpp"
)

target_link_libraries(soteria-qualification-tests
    PRIVATE
        soteria-core
        GTest::gtest_main
)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(soteria-qualification-tests PRIVATE /w44100)
else()
    target_compile_options(soteria-qualification-tests PRIVATE
        -Wno-global-constructors
    )
endif()

include(GoogleTest)
gtest_discover_tests(soteria-qualification-tests
    TEST_PREFIX "qualification."
    PROPERTIES LABELS "qualification"
)
