file(GLOB INTEGRATION_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(soteria-integration-tests ${INTEGRATION_SOURCES})

target_include_directories(soteria-integration-tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}          # project root → #include "engine/core/vksc_context.hpp"
)

target_link_libraries(soteria-integration-tests
    PRIVATE
        soteria-core
        GTest::gtest_main
)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(soteria-integration-tests PRIVATE /w44100)
else()
    target_compile_options(soteria-integration-tests PRIVATE
        -Wno-global-constructors
    )
endif()

include(GoogleTest)
gtest_discover_tests(soteria-integration-tests
    TEST_PREFIX "integration."
    PROPERTIES LABELS "integration"
)
