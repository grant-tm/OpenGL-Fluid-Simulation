#ifndef SIMULATION_DATA_H
#define SIMULATION_DATA_H

#include "base.h"
#include "opengl_helpers.h"
#include "simulation_spawner.h"

typedef struct SimulationSettingsGpu
{
    Vec4 gravity_and_delta_time;
    Vec4 bounds_size_and_collision_damping;
    Vec4 density_parameters;
    Vec4 viscosity_and_miscellaneous;
} SimulationSettingsGpu;

typedef struct SimulationParticleBuffers
{
    OpenGLBuffer position_buffer;
    OpenGLBuffer predicted_position_buffer;
    OpenGLBuffer velocity_buffer;
    OpenGLBuffer density_buffer;
    OpenGLBuffer whitewater_spawn_debug_buffer;
    OpenGLBuffer sort_target_position_buffer;
    OpenGLBuffer sort_target_predicted_position_buffer;
    OpenGLBuffer sort_target_velocity_buffer;
    OpenGLBuffer spatial_key_buffer;
    OpenGLBuffer spatial_hash_buffer;
    OpenGLBuffer spatial_index_buffer;
    OpenGLBuffer spatial_offset_buffer;
    u32 particle_count;
} SimulationParticleBuffers;

bool Simulation_CreateParticleBuffers (SimulationParticleBuffers *particle_buffers, u32 particle_count);
bool Simulation_CreateParticleBuffersFromSpawnData (SimulationParticleBuffers *particle_buffers, const SimulationSpawnData *spawn_data);
void Simulation_DestroyParticleBuffers (SimulationParticleBuffers *particle_buffers);
bool Simulation_RunDataModelValidation (void);

#endif // SIMULATION_DATA_H
