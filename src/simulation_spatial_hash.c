#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_spatial_hash.h"

typedef struct SpatialKeyIndexPair
{
    u32 key;
    u32 hash;
    u32 index;
} SpatialKeyIndexPair;

static int SimulationSpatialHash_ComparePairs (const void *left_value, const void *right_value)
{
    const SpatialKeyIndexPair *left_pair = (const SpatialKeyIndexPair *) left_value;
    const SpatialKeyIndexPair *right_pair = (const SpatialKeyIndexPair *) right_value;

    if (left_pair->key < right_pair->key) return -1;
    if (left_pair->key > right_pair->key) return 1;
    if (left_pair->hash < right_pair->hash) return -1;
    if (left_pair->hash > right_pair->hash) return 1;
    if (left_pair->index < right_pair->index) return -1;
    if (left_pair->index > right_pair->index) return 1;
    return 0;
}

static bool SimulationSpatialHash_SortAndUpload (
    SimulationParticleBuffers *particle_buffers,
    u32 *unsorted_hashes,
    u32 *unsorted_keys,
    u32 *unsorted_indices)
{
    u32 particle_count = particle_buffers->particle_count;
    SpatialKeyIndexPair *sorted_pairs = (SpatialKeyIndexPair *) calloc(particle_count, sizeof(SpatialKeyIndexPair));
    u32 *sorted_hashes = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *sorted_keys = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *sorted_indices = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *offsets = (u32 *) malloc((size_t) particle_count * sizeof(u32));

    if (sorted_pairs == NULL || sorted_hashes == NULL || sorted_keys == NULL || sorted_indices == NULL || offsets == NULL)
    {
        free(sorted_pairs);
        free(sorted_hashes);
        free(sorted_keys);
        free(sorted_indices);
        free(offsets);
        Base_LogError("Failed to allocate spatial hash sorting buffers.");
        return false;
    }

    for (u32 particle_index = 0; particle_index < particle_count; particle_index++)
    {
        sorted_pairs[particle_index].key = unsorted_keys[particle_index];
        sorted_pairs[particle_index].hash = unsorted_hashes[particle_index];
        sorted_pairs[particle_index].index = unsorted_indices[particle_index];
        offsets[particle_index] = particle_count;
    }

    qsort(sorted_pairs, particle_count, sizeof(SpatialKeyIndexPair), SimulationSpatialHash_ComparePairs);

    for (u32 sorted_index = 0; sorted_index < particle_count; sorted_index++)
    {
        u32 key = sorted_pairs[sorted_index].key;
        u32 hash = sorted_pairs[sorted_index].hash;
        u32 source_index = sorted_pairs[sorted_index].index;
        sorted_hashes[sorted_index] = hash;
        sorted_keys[sorted_index] = key;
        sorted_indices[sorted_index] = source_index;

        if (key < particle_count && offsets[key] == particle_count)
        {
            offsets[key] = sorted_index;
        }
    }

    bool upload_success =
        OpenGL_UpdateBuffer(&particle_buffers->spatial_key_buffer, 0, (i32) (particle_count * sizeof(u32)), sorted_keys) &&
        OpenGL_UpdateBuffer(&particle_buffers->spatial_hash_buffer, 0, (i32) (particle_count * sizeof(u32)), sorted_hashes) &&
        OpenGL_UpdateBuffer(&particle_buffers->spatial_index_buffer, 0, (i32) (particle_count * sizeof(u32)), sorted_indices) &&
        OpenGL_UpdateBuffer(&particle_buffers->spatial_offset_buffer, 0, (i32) (particle_count * sizeof(u32)), offsets);

    free(sorted_pairs);
    free(sorted_hashes);
    free(sorted_keys);
    free(sorted_indices);
    free(offsets);
    return upload_success;
}

bool SimulationSpatialHash_Initialize (SimulationSpatialHash *spatial_hash)
{
    Base_Assert(spatial_hash != NULL);
    memset(spatial_hash, 0, sizeof(*spatial_hash));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    spatial_hash->generate_keys_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/spatial_hash_keys.comp");
    if (spatial_hash->generate_keys_program_identifier == 0)
    {
        Base_LogError("Failed to create the spatial hash key-generation compute program.");
        return false;
    }

    spatial_hash->cell_size_uniform = glGetUniformLocation(spatial_hash->generate_keys_program_identifier, "u_cell_size");
    spatial_hash->particle_count_uniform = glGetUniformLocation(spatial_hash->generate_keys_program_identifier, "u_particle_count");
    spatial_hash->table_size_uniform = glGetUniformLocation(spatial_hash->generate_keys_program_identifier, "u_table_size");
    return true;
}

void SimulationSpatialHash_Shutdown (SimulationSpatialHash *spatial_hash)
{
    if (spatial_hash == NULL) return;

    if (spatial_hash->generate_keys_program_identifier != 0)
    {
        glDeleteProgram(spatial_hash->generate_keys_program_identifier);
    }

    memset(spatial_hash, 0, sizeof(*spatial_hash));
}

bool SimulationSpatialHash_Run (
    SimulationSpatialHash *spatial_hash,
    SimulationParticleBuffers *particle_buffers,
    SimulationSpatialHashSettings settings)
{
    Base_Assert(spatial_hash != NULL);
    Base_Assert(particle_buffers != NULL);

    if (spatial_hash->generate_keys_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return false;
    if (settings.cell_size <= 0.0f)
    {
        Base_LogError("Spatial hash cell size must be greater than zero.");
        return false;
    }

    u32 particle_count = particle_buffers->particle_count;
    u32 workgroup_count_x = (particle_count + 63u) / 64u;

    glUseProgram(spatial_hash->generate_keys_program_identifier);
    glUniform1f(spatial_hash->cell_size_uniform, settings.cell_size);
    glUniform1i(spatial_hash->particle_count_uniform, (i32) particle_count);
    glUniform1i(spatial_hash->table_size_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, particle_buffers->spatial_index_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, 0);
    glUseProgram(0);

    u32 *unsorted_hashes = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *unsorted_keys = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *unsorted_indices = (u32 *) calloc(particle_count, sizeof(u32));
    if (unsorted_hashes == NULL || unsorted_keys == NULL || unsorted_indices == NULL)
    {
        free(unsorted_hashes);
        free(unsorted_keys);
        free(unsorted_indices);
        Base_LogError("Failed to allocate spatial hash readback buffers.");
        return false;
    }

    bool read_success =
        OpenGL_ReadBuffer(&particle_buffers->spatial_hash_buffer, unsorted_hashes, (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(&particle_buffers->spatial_key_buffer, unsorted_keys, (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(&particle_buffers->spatial_index_buffer, unsorted_indices, (i32) (particle_count * sizeof(u32)));

    if (!read_success)
    {
        free(unsorted_hashes);
        free(unsorted_keys);
        free(unsorted_indices);
        return false;
    }

    bool upload_success = SimulationSpatialHash_SortAndUpload(particle_buffers, unsorted_hashes, unsorted_keys, unsorted_indices);
    free(unsorted_hashes);
    free(unsorted_keys);
    free(unsorted_indices);
    return upload_success;
}

bool SimulationSpatialHash_RunValidation (void)
{
    SimulationSpawnBox spawn_box = {0};
    spawn_box.center = Vec3_Create(0.0f, 0.0f, 0.0f);
    spawn_box.size = Vec3_Create(4.0f, 4.0f, 2.0f);
    spawn_box.particle_spacing = 1.0f;
    spawn_box.initial_velocity = Vec3_Create(0.0f, 0.0f, 0.0f);

    SimulationSpawnData spawn_data = {0};
    if (!Simulation_GenerateSpawnDataBox(&spawn_data, spawn_box))
    {
        return false;
    }

    SimulationParticleBuffers particle_buffers = {0};
    if (!Simulation_CreateParticleBuffersFromSpawnData(&particle_buffers, &spawn_data))
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationSpatialHash spatial_hash = {0};
    if (!SimulationSpatialHash_Initialize(&spatial_hash))
    {
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationSpatialHashSettings settings = {0};
    settings.cell_size = 1.0f;
    bool run_success = SimulationSpatialHash_Run(&spatial_hash, &particle_buffers, settings);

    u32 particle_count = particle_buffers.particle_count;
    u32 *sorted_hashes = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *sorted_keys = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *sorted_indices = (u32 *) calloc(particle_count, sizeof(u32));
    u32 *offsets = (u32 *) calloc(particle_count, sizeof(u32));

    bool validation_success = run_success &&
        sorted_hashes != NULL &&
        sorted_keys != NULL &&
        sorted_indices != NULL &&
        offsets != NULL &&
        OpenGL_ReadBuffer(&particle_buffers.spatial_hash_buffer, sorted_hashes, (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(&particle_buffers.spatial_key_buffer, sorted_keys, (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(&particle_buffers.spatial_index_buffer, sorted_indices, (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(&particle_buffers.spatial_offset_buffer, offsets, (i32) (particle_count * sizeof(u32)));

    if (validation_success)
    {
        for (u32 sorted_index = 0; sorted_index < particle_count; sorted_index++)
        {
            if (sorted_indices[sorted_index] >= particle_count)
            {
                validation_success = false;
                break;
            }

            if (sorted_index > 0 && sorted_keys[sorted_index - 1] > sorted_keys[sorted_index])
            {
                validation_success = false;
                break;
            }

            if (sorted_index > 0 &&
                sorted_keys[sorted_index - 1] == sorted_keys[sorted_index] &&
                sorted_hashes[sorted_index - 1] > sorted_hashes[sorted_index])
            {
                validation_success = false;
                break;
            }
        }
    }

    if (validation_success)
    {
        for (u32 key = 0; key < particle_count; key++)
        {
            if (offsets[key] < particle_count)
            {
                if (sorted_keys[offsets[key]] != key)
                {
                    validation_success = false;
                    break;
                }
            }
        }
    }

    free(sorted_hashes);
    free(sorted_keys);
    free(sorted_indices);
    free(offsets);
    SimulationSpatialHash_Shutdown(&spatial_hash);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Spatial hash validation failed.");
        return false;
    }

    Base_LogInfo("Spatial hash validation passed.");
    return true;
}
