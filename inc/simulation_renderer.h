#ifndef SIMULATION_RENDERER_H
#define SIMULATION_RENDERER_H

#include "base.h"
#include "simulation_data.h"
#include "simulation_volume_density.h"

typedef struct SimulationCamera
{
    f32 yaw_radians;
    f32 pitch_radians;
    f32 distance;
    Vec3 target;
} SimulationCamera;

typedef struct SimulationRenderer
{
    u32 particle_program_identifier;
    u32 line_program_identifier;
    u32 volume_program_identifier;
    u32 screen_fluid_thickness_program_identifier;
    u32 screen_fluid_blur_program_identifier;
    u32 screen_fluid_composite_program_identifier;
    u32 particle_vao_identifier;
    u32 bounds_vao_identifier;
    u32 bounds_vbo_identifier;
    u32 volume_vao_identifier;
    u32 volume_vbo_identifier;
    u32 fullscreen_vao_identifier;
    u32 screen_fluid_framebuffer_identifier;
    u32 screen_fluid_blur_framebuffer_identifier;
    u32 screen_fluid_depth_renderbuffer_identifier;
    u32 screen_fluid_thickness_texture_identifier;
    u32 screen_fluid_blur_texture_identifier;
    i32 screen_fluid_texture_width;
    i32 screen_fluid_texture_height;
    i32 particle_projection_uniform;
    i32 particle_view_uniform;
    i32 particle_color_uniform;
    i32 particle_size_uniform;
    i32 particle_visualization_mode_uniform;
    i32 particle_density_minimum_uniform;
    i32 particle_density_maximum_uniform;
    i32 particle_speed_minimum_uniform;
    i32 particle_speed_maximum_uniform;
    i32 line_projection_uniform;
    i32 line_view_uniform;
    i32 line_color_uniform;
    i32 line_model_uniform;
    i32 volume_projection_uniform;
    i32 volume_view_uniform;
    i32 volume_bounds_size_uniform;
    i32 volume_camera_position_uniform;
    i32 volume_density_texture_uniform;
    i32 volume_step_count_uniform;
    i32 volume_density_multiplier_uniform;
    i32 volume_voxel_size_uniform;
    i32 screen_fluid_thickness_projection_uniform;
    i32 screen_fluid_thickness_view_uniform;
    i32 screen_fluid_thickness_point_size_uniform;
    i32 screen_fluid_blur_texture_uniform;
    i32 screen_fluid_blur_direction_uniform;
    i32 screen_fluid_blur_texel_size_uniform;
    i32 screen_fluid_composite_texture_uniform;
    i32 screen_fluid_composite_texel_size_uniform;
} SimulationRenderer;

typedef enum SimulationRenderMode
{
    SIMULATION_RENDER_MODE_PARTICLES = 0,
    SIMULATION_RENDER_MODE_VOLUME = 1,
    SIMULATION_RENDER_MODE_SCREEN_FLUID = 2,
} SimulationRenderMode;

typedef enum SimulationParticleVisualizationMode
{
    SIMULATION_PARTICLE_VISUALIZATION_BASIC = 0,
    SIMULATION_PARTICLE_VISUALIZATION_DENSITY = 1,
    SIMULATION_PARTICLE_VISUALIZATION_VELOCITY = 2,
    SIMULATION_PARTICLE_VISUALIZATION_SPATIAL_KEY = 3,
} SimulationParticleVisualizationMode;

bool SimulationRenderer_Initialize (SimulationRenderer *renderer, const SimulationParticleBuffers *particle_buffers);
void SimulationRenderer_Shutdown (SimulationRenderer *renderer);
void SimulationRenderer_UpdateCamera (SimulationCamera *camera, f32 delta_time_seconds);
void SimulationRenderer_LogScreenFluidReadback (SimulationRenderer *renderer);
void SimulationRenderer_Render (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    const SimulationVolumeDensity *volume_density,
    SimulationCamera camera,
    Vec3 simulation_bounds_size,
    SimulationRenderMode render_mode,
    SimulationParticleVisualizationMode particle_visualization_mode,
    f32 density_minimum,
    f32 density_maximum,
    f32 speed_minimum,
    f32 speed_maximum,
    i32 viewport_width,
    i32 viewport_height);

#endif // SIMULATION_RENDERER_H
