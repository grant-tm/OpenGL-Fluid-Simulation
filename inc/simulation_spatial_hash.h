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
    u32 clear_counts_program_identifier;
    u32 calculate_counts_program_identifier;
    u32 scatter_program_identifier;
    u32 copyback_program_identifier;
    u32 scan_block_program_identifier;
    u32 scan_combine_program_identifier;
    u32 initialize_offsets_program_identifier;
    u32 calculate_offsets_program_identifier;
    u32 reorder_hashes_program_identifier;
    u32 copyback_hashes_program_identifier;
    OpenGLBuffer sorted_key_buffer;
    OpenGLBuffer sorted_index_buffer;
    OpenGLBuffer sorted_hash_buffer;
    OpenGLBuffer counts_buffer;
    OpenGLBuffer group_sums_buffer;
    OpenGLBuffer group_sums_scratch_buffer;
    u32 particle_capacity;
    u32 group_sum_capacity;
    i32 clear_counts_particle_count_uniform;
    i32 calculate_counts_particle_count_uniform;
    i32 scatter_particle_count_uniform;
    i32 copyback_particle_count_uniform;
    i32 scan_item_count_uniform;
    i32 scan_combine_item_count_uniform;
    i32 initialize_offsets_particle_count_uniform;
    i32 calculate_offsets_particle_count_uniform;
    i32 reorder_hashes_particle_count_uniform;
    i32 copyback_hashes_particle_count_uniform;
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
