#ifndef SIMULATION_SPATIAL_HASH_H
#define SIMULATION_SPATIAL_HASH_H

#include "base.h"
#include "simulation_data.h"

typedef struct SimulationSpatialHashSettings
{
    f32 cell_size;
} SimulationSpatialHashSettings;

typedef struct SimulationSpatialHash
{
    u32 generate_keys_program_identifier;
    i32 cell_size_uniform;
    i32 particle_count_uniform;
    i32 table_size_uniform;
} SimulationSpatialHash;

bool SimulationSpatialHash_Initialize (SimulationSpatialHash *spatial_hash);
void SimulationSpatialHash_Shutdown (SimulationSpatialHash *spatial_hash);
bool SimulationSpatialHash_Run (
    SimulationSpatialHash *spatial_hash,
    SimulationParticleBuffers *particle_buffers,
    SimulationSpatialHashSettings settings);
bool SimulationSpatialHash_RunValidation (void);

#endif // SIMULATION_SPATIAL_HASH_H
