#ifndef SIMULATION_RENDERER_H
#define SIMULATION_RENDERER_H

#include "base.h"
#include "simulation_data.h"

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
    u32 particle_vao_identifier;
    u32 bounds_vao_identifier;
    u32 bounds_vbo_identifier;
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
} SimulationRenderer;

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
void SimulationRenderer_Render (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    SimulationCamera camera,
    Vec3 simulation_bounds_size,
    SimulationParticleVisualizationMode particle_visualization_mode,
    f32 density_minimum,
    f32 density_maximum,
    f32 speed_minimum,
    f32 speed_maximum,
    i32 viewport_width,
    i32 viewport_height);

#endif // SIMULATION_RENDERER_H
