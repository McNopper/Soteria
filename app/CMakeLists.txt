add_executable(soteria-sim
    main.cpp
    rendering/horizon_renderer.cpp
    rendering/horizon_geometry.cpp
)

target_include_directories(soteria-sim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}      # app/ (for demo/, rendering/)
        ${CMAKE_CURRENT_SOURCE_DIR}/..   # project root (engine/ lives here)
)

target_link_libraries(soteria-sim
    PRIVATE
        soteria-core
)

# Copy DLLs required at runtime to the output directory.
# vulkansc-1.dll  — the Vulkan SC loader.
# vksconvk.dll    — emulation layer (SC → Vulkan 1.2).
add_custom_command(TARGET soteria-sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:VulkanSC::Loader>"
        "$<TARGET_FILE_DIR:soteria-sim>"
    COMMENT "Copying vulkansc-1.dll to output directory"
)

add_custom_command(TARGET soteria-sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VulkanSC_INCLUDE_DIR}/../bin/vksconvk.dll"
        "$<TARGET_FILE_DIR:soteria-sim>"
    COMMENT "Copying vksconvk.dll (emulation layer) to output directory"
)
