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

soteria_stage_runtime_dlls(soteria-sim)
