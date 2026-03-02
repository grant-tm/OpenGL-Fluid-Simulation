#ifndef SIMULATION_WHITEWATER_H
#define SIMULATION_WHITEWATER_H

#include "simulation_data.h"

typedef struct SimulationWhitewaterSettings
{
    u32 maximum_particle_count;
    f32 spawn_rate;
    f32 spawn_rate_fade_in_time;
    f32 spawn_rate_fade_start_time;
    f32 trapped_air_velocity_minimum;
    f32 trapped_air_velocity_maximum;
    f32 kinetic_energy_minimum;
    f32 kinetic_energy_maximum;
    f32 target_density;
    f32 smoothing_radius;
    f32 gravity;
    f32 delta_time_seconds;
    f32 bubble_buoyancy;
    i32 spray_classify_maximum_neighbors;
    i32 bubble_classify_minimum_neighbors;
    f32 bubble_scale;
    f32 bubble_scale_change_speed;
    f32 collision_damping;
    Vec3 bounds_size;
    f32 simulation_time_seconds;
} SimulationWhitewaterSettings;

typedef struct SimulationWhitewaterParticleGpu
{
    Vec4 position_and_scale;
    Vec4 velocity_and_lifetime;
    Vec4 classification_data;
} SimulationWhitewaterParticleGpu;

typedef struct SimulationWhitewater
{
    OpenGLBuffer particle_buffer;
    OpenGLBuffer compacted_particle_buffer;
    OpenGLBuffer counter_buffer;
    u32 maximum_particle_count;
    u32 spawn_program_identifier;
    u32 update_program_identifier;
    u32 copyback_program_identifier;
    i32 spawn_particle_count_uniform;
    i32 spawn_table_size_uniform;
    i32 spawn_maximum_whitewater_count_uniform;
    i32 spawn_delta_time_uniform;
    i32 spawn_simulation_time_uniform;
    i32 spawn_spawn_rate_uniform;
    i32 spawn_trapped_air_velocity_minimum_uniform;
    i32 spawn_trapped_air_velocity_maximum_uniform;
    i32 spawn_kinetic_energy_minimum_uniform;
    i32 spawn_kinetic_energy_maximum_uniform;
    i32 spawn_bubble_scale_uniform;
    i32 spawn_target_density_uniform;
    i32 spawn_smoothing_radius_uniform;
    i32 update_particle_count_uniform;
    i32 update_table_size_uniform;
    i32 update_maximum_whitewater_count_uniform;
    i32 update_delta_time_uniform;
    i32 update_gravity_uniform;
    i32 update_bubble_buoyancy_uniform;
    i32 update_smoothing_radius_uniform;
    i32 update_spray_classify_maximum_neighbors_uniform;
    i32 update_bubble_classify_minimum_neighbors_uniform;
    i32 update_bubble_scale_uniform;
    i32 update_bubble_scale_change_speed_uniform;
    i32 update_collision_damping_uniform;
    i32 update_bounds_size_uniform;
    i32 copyback_maximum_whitewater_count_uniform;
} SimulationWhitewater;

bool SimulationWhitewater_Initialize (SimulationWhitewater *whitewater, u32 maximum_particle_count);
void SimulationWhitewater_Shutdown (SimulationWhitewater *whitewater);
bool SimulationWhitewater_Reset (SimulationWhitewater *whitewater);
bool SimulationWhitewater_Run (
    SimulationWhitewater *whitewater,
    const SimulationParticleBuffers *particle_buffers,
    SimulationWhitewaterSettings settings);
bool SimulationWhitewater_UpdateOnly (
    SimulationWhitewater *whitewater,
    const SimulationParticleBuffers *particle_buffers,
    SimulationWhitewaterSettings settings);
bool SimulationWhitewater_ReadActiveParticleCount (SimulationWhitewater *whitewater, u32 *active_particle_count);
bool SimulationWhitewater_RunValidation (void);

#endif // SIMULATION_WHITEWATER_H
