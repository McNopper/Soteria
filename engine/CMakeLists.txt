add_library(soteria-core STATIC
    core/log.cpp
    core/vksc_context.cpp
    wsi/display_output.cpp
    rendering/swapchain.cpp
    rendering/pipeline_cache.cpp
    rendering/vertex_buffer.cpp
    rendering/frame_sync.cpp
    rendering/command_pool.cpp
)

target_include_directories(soteria-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(soteria-core
    PUBLIC
        VulkanSC::VulkanSC
)
