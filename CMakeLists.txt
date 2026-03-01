cmake_minimum_required(VERSION 3.20)

project(FluidSim LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(FluidSim
    src/base.c
    src/glad.c
    src/main.c
    src/simulation_collision.c
    src/simulation_density.c
    src/simulation_pipeline.c
    src/simulation_pressure.c
    src/simulation_viscosity.c
    src/opengl_helpers.c
    src/simulation_data.c
    src/simulation_reorder.c
    src/simulation_renderer.c
    src/simulation_spawner.c
    src/simulation_spatial_hash.c
    src/simulation_step.c
)

target_include_directories(FluidSim PRIVATE
    inc
)

target_compile_options(FluidSim PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
)

target_link_libraries(FluidSim PRIVATE
    opengl32
    user32
    gdi32
)

set_target_properties(FluidSim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET FluidSim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:FluidSim>/assets"
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT FluidSim)
