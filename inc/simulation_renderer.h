#ifndef SIMULATION_RENDERER_H
#define SIMULATION_RENDERER_H

#include "base.h"
#include "simulation_data.h"
#include "simulation_volume_density.h"
#include "simulation_whitewater.h"

#define SIMULATION_RENDERER_GPU_QUERY_RING_SIZE 4

typedef struct SimulationCamera
{
    f32 yaw_radians;
    f32 pitch_radians;
    f32 distance;
    Vec3 target;
} SimulationCamera;

typedef struct SimulationScaleModel
{
    f32 particle_spacing;
    f32 smoothing_radius;
    f32 screen_fluid_thickness_particle_radius;
    f32 screen_fluid_depth_particle_radius;
    f32 screen_fluid_blur_world_radius;
    f32 whitewater_billboard_scale;
} SimulationScaleModel;

typedef struct SimulationRenderer
{
    struct SimulationRendererDebugTimings
    {
        f64 bounds_milliseconds;
        f64 particles_milliseconds;
        f64 scene_copy_milliseconds;
        f64 shadow_milliseconds;
        f64 surface_inputs_milliseconds;
        f64 prepare_milliseconds;
        f64 blur_milliseconds;
        f64 normal_milliseconds;
        f64 composite_milliseconds;
        f64 whitewater_milliseconds;
        f64 total_milliseconds;
        f64 gpu_scene_copy_milliseconds;
        f64 gpu_shadow_milliseconds;
        f64 gpu_surface_inputs_milliseconds;
        f64 gpu_blur_milliseconds;
        f64 gpu_normal_milliseconds;
        f64 gpu_composite_milliseconds;
        f64 gpu_total_milliseconds;
    } last_debug_timings;
    u32 gpu_scene_copy_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_shadow_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_surface_inputs_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_blur_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_normal_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_composite_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_total_start_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_total_end_query_identifiers[SIMULATION_RENDERER_GPU_QUERY_RING_SIZE];
    u32 gpu_query_frame_index;
    u32 particle_program_identifier;
    u32 line_program_identifier;
    u32 whitewater_program_identifier;
    u32 screen_fluid_thickness_program_identifier;
    u32 screen_fluid_depth_program_identifier;
    u32 screen_fluid_prepare_program_identifier;
    u32 screen_fluid_blur_program_identifier;
    u32 screen_fluid_normal_program_identifier;
    u32 screen_fluid_debug_program_identifier;
    u32 screen_fluid_shadow_blur_program_identifier;
    u32 screen_fluid_composite_program_identifier;
    u32 screen_fluid_airborne_foam_program_identifier;
    u32 particle_vao_identifier;
    u32 screen_fluid_quad_vao_identifier;
    u32 screen_fluid_quad_vbo_identifier;
    u32 bounds_vao_identifier;
    u32 bounds_vbo_identifier;
    u32 whitewater_vao_identifier;
    u32 whitewater_quad_vbo_identifier;
    u32 fullscreen_vao_identifier;
    u32 screen_fluid_framebuffer_identifier;
    u32 screen_fluid_blur_framebuffer_identifier;
    u32 screen_fluid_depth_renderbuffer_identifier;
    u32 screen_fluid_thickness_texture_identifier;
    u32 screen_fluid_depth_texture_identifier;
    u32 screen_fluid_depth_blur_texture_identifier;
    u32 screen_fluid_blur_texture_identifier;
    u32 screen_fluid_comp_texture_identifier;
    u32 screen_fluid_comp_blur_texture_identifier;
    u32 screen_fluid_normal_texture_identifier;
    u32 screen_fluid_scene_texture_identifier;
    u32 screen_fluid_foam_texture_identifier;
    u32 screen_fluid_shadow_texture_identifier;
    u32 screen_fluid_shadow_blur_texture_identifier;
    i32 screen_fluid_shadow_texture_width;
    i32 screen_fluid_shadow_texture_height;
    i32 screen_fluid_thickness_texture_width;
    i32 screen_fluid_thickness_texture_height;
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
    i32 whitewater_projection_uniform;
    i32 whitewater_view_uniform;
    i32 whitewater_scale_uniform;
    i32 screen_fluid_thickness_projection_uniform;
    i32 screen_fluid_thickness_view_uniform;
    i32 screen_fluid_thickness_point_size_uniform;
    i32 screen_fluid_depth_projection_uniform;
    i32 screen_fluid_depth_view_uniform;
    i32 screen_fluid_depth_point_size_uniform;
    i32 screen_fluid_depth_viewport_height_uniform;
    i32 screen_fluid_prepare_depth_texture_uniform;
    i32 screen_fluid_prepare_thickness_texture_uniform;
    i32 screen_fluid_blur_texture_uniform;
    i32 screen_fluid_blur_filter_mode_uniform;
    i32 screen_fluid_blur_depth_texture_uniform;
    i32 screen_fluid_blur_direction_uniform;
    i32 screen_fluid_blur_texel_size_uniform;
    i32 screen_fluid_blur_projection_scale_uniform;
    i32 screen_fluid_blur_image_width_uniform;
    i32 screen_fluid_blur_world_radius_uniform;
    i32 screen_fluid_blur_max_radius_uniform;
    i32 screen_fluid_blur_strength_uniform;
    i32 screen_fluid_blur_difference_strength_uniform;
    i32 screen_fluid_normal_comp_texture_uniform;
    i32 screen_fluid_normal_texel_size_uniform;
    i32 screen_fluid_normal_projection_uniform;
    i32 screen_fluid_normal_inverse_projection_uniform;
    i32 screen_fluid_debug_texture_uniform;
    i32 screen_fluid_debug_mode_uniform;
    i32 screen_fluid_shadow_blur_texture_uniform;
    i32 screen_fluid_shadow_blur_direction_uniform;
    i32 screen_fluid_shadow_blur_texel_size_uniform;
    i32 screen_fluid_composite_texture_uniform;
    i32 screen_fluid_composite_depth_texture_uniform;
    i32 screen_fluid_composite_texel_size_uniform;
    i32 screen_fluid_composite_projection_uniform;
    i32 screen_fluid_composite_inverse_projection_uniform;
    i32 screen_fluid_composite_inverse_view_uniform;
    i32 screen_fluid_composite_bounds_size_uniform;
    i32 screen_fluid_composite_normal_texture_uniform;
    i32 screen_fluid_composite_scene_texture_uniform;
    i32 screen_fluid_composite_foam_texture_uniform;
    i32 screen_fluid_composite_shadow_texture_uniform;
    i32 screen_fluid_composite_shadow_view_projection_uniform;
    i32 screen_fluid_airborne_foam_fluid_texture_uniform;
    i32 screen_fluid_airborne_foam_foam_texture_uniform;
} SimulationRenderer;

typedef enum SimulationRenderMode
{
    SIMULATION_RENDER_MODE_PARTICLES = 0,
    SIMULATION_RENDER_MODE_SCREEN_FLUID = 1,
} SimulationRenderMode;

typedef enum SimulationParticleVisualizationMode
{
    SIMULATION_PARTICLE_VISUALIZATION_BASIC = 0,
    SIMULATION_PARTICLE_VISUALIZATION_DENSITY = 1,
    SIMULATION_PARTICLE_VISUALIZATION_VELOCITY = 2,
    SIMULATION_PARTICLE_VISUALIZATION_SPATIAL_KEY = 3,
} SimulationParticleVisualizationMode;

typedef enum SimulationScreenFluidVisualizationMode
{
    SIMULATION_SCREEN_FLUID_VISUALIZATION_COMPOSITE = 0,
    SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED = 1,
    SIMULATION_SCREEN_FLUID_VISUALIZATION_NORMAL = 2,
    SIMULATION_SCREEN_FLUID_VISUALIZATION_SMOOTH_DEPTH = 3,
    SIMULATION_SCREEN_FLUID_VISUALIZATION_HARD_DEPTH = 4,
    SIMULATION_SCREEN_FLUID_VISUALIZATION_DEPTH_DELTA = 5,
} SimulationScreenFluidVisualizationMode;

typedef enum SimulationScreenFluidSmoothingMode
{
    SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL = 0,
    SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN = 1,
    SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D = 2,
} SimulationScreenFluidSmoothingMode;

bool SimulationRenderer_Initialize (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    const SimulationWhitewater *whitewater);
void SimulationRenderer_Shutdown (SimulationRenderer *renderer);
void SimulationRenderer_UpdateCamera (SimulationCamera *camera, f32 delta_time_seconds);
void SimulationRenderer_LogScreenFluidReadback (SimulationRenderer *renderer);
void SimulationRenderer_Render (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    const SimulationWhitewater *whitewater,
    const SimulationVolumeDensity *volume_density,
    SimulationCamera camera,
    Vec3 simulation_bounds_size,
    SimulationScaleModel scale_model,
    SimulationRenderMode render_mode,
    SimulationParticleVisualizationMode particle_visualization_mode,
    SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode,
    SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode,
    f32 density_minimum,
    f32 density_maximum,
    f32 speed_minimum,
    f32 speed_maximum,
    i32 viewport_width,
    i32 viewport_height);

#endif // SIMULATION_RENDERER_H
