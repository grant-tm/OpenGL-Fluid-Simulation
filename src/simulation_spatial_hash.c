#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_spatial_hash.h"

#define SIMULATION_SORT_THREAD_GROUP_SIZE 256u
#define SIMULATION_SCAN_ITEMS_PER_GROUP (SIMULATION_SORT_THREAD_GROUP_SIZE * 2u)

static u32 SimulationSpatialHash_CeilDivideU32 (u32 numerator, u32 denominator)
{
    Base_Assert(denominator != 0u);
    return (numerator + denominator - 1u) / denominator;
}

static bool SimulationSpatialHash_CreateTemporaryBuffer (OpenGLBuffer *buffer, u32 element_count)
{
    Base_Assert(buffer != NULL);
    return OpenGL_CreateBuffer(
        buffer,
        GL_SHADER_STORAGE_BUFFER,
        (i32) (element_count * sizeof(u32)),
        NULL,
        GL_DYNAMIC_DRAW);
}

static bool SimulationSpatialHash_EnsureTemporaryBuffers (
    SimulationSpatialHash *spatial_hash,
    u32 particle_count)
{
    Base_Assert(spatial_hash != NULL);

    if (spatial_hash->particle_capacity == particle_count &&
        spatial_hash->group_sum_capacity >= SimulationSpatialHash_CeilDivideU32(particle_count, SIMULATION_SCAN_ITEMS_PER_GROUP) &&
        spatial_hash->sorted_key_buffer.identifier != 0 &&
        spatial_hash->sorted_index_buffer.identifier != 0 &&
        spatial_hash->sorted_hash_buffer.identifier != 0 &&
        spatial_hash->counts_buffer.identifier != 0 &&
        spatial_hash->group_sums_buffer.identifier != 0 &&
        spatial_hash->group_sums_scratch_buffer.identifier != 0)
    {
        return true;
    }

    OpenGL_DestroyBuffer(&spatial_hash->sorted_key_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->sorted_index_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->sorted_hash_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->counts_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->group_sums_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->group_sums_scratch_buffer);
    spatial_hash->particle_capacity = 0u;
    spatial_hash->group_sum_capacity = 0u;

    u32 group_sum_count = SimulationSpatialHash_CeilDivideU32(particle_count, SIMULATION_SCAN_ITEMS_PER_GROUP);
    if (group_sum_count == 0u)
    {
        group_sum_count = 1u;
    }

    u32 group_sum_scratch_count = SimulationSpatialHash_CeilDivideU32(group_sum_count, SIMULATION_SCAN_ITEMS_PER_GROUP);
    if (group_sum_scratch_count == 0u)
    {
        group_sum_scratch_count = 1u;
    }

    bool create_success =
        SimulationSpatialHash_CreateTemporaryBuffer(&spatial_hash->sorted_key_buffer, particle_count) &&
        SimulationSpatialHash_CreateTemporaryBuffer(&spatial_hash->sorted_index_buffer, particle_count) &&
        SimulationSpatialHash_CreateTemporaryBuffer(&spatial_hash->sorted_hash_buffer, particle_count) &&
        SimulationSpatialHash_CreateTemporaryBuffer(&spatial_hash->counts_buffer, particle_count) &&
        SimulationSpatialHash_CreateTemporaryBuffer(&spatial_hash->group_sums_buffer, group_sum_count) &&
        SimulationSpatialHash_CreateTemporaryBuffer(&spatial_hash->group_sums_scratch_buffer, group_sum_scratch_count);

    if (!create_success)
    {
        Base_LogError("Failed to allocate GPU spatial hash temporary buffers.");
        OpenGL_DestroyBuffer(&spatial_hash->sorted_key_buffer);
        OpenGL_DestroyBuffer(&spatial_hash->sorted_index_buffer);
        OpenGL_DestroyBuffer(&spatial_hash->sorted_hash_buffer);
        OpenGL_DestroyBuffer(&spatial_hash->counts_buffer);
        OpenGL_DestroyBuffer(&spatial_hash->group_sums_buffer);
        OpenGL_DestroyBuffer(&spatial_hash->group_sums_scratch_buffer);
        return false;
    }

    spatial_hash->particle_capacity = particle_count;
    spatial_hash->group_sum_capacity = group_sum_count;
    return true;
}

static bool SimulationSpatialHash_RunExclusiveScan (
    SimulationSpatialHash *spatial_hash,
    OpenGLBuffer *elements_buffer,
    u32 item_count)
{
    Base_Assert(spatial_hash != NULL);
    Base_Assert(elements_buffer != NULL);

    if (item_count == 0u)
    {
        return true;
    }

    u32 group_count = SimulationSpatialHash_CeilDivideU32(item_count, SIMULATION_SCAN_ITEMS_PER_GROUP);

    glUseProgram(spatial_hash->scan_block_program_identifier);
    glUniform1i(spatial_hash->scan_item_count_uniform, (i32) item_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, elements_buffer->identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spatial_hash->group_sums_buffer.identifier);
    OpenGL_DispatchCompute(group_count, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    if (group_count > 1u)
    {
        u32 parent_group_count = SimulationSpatialHash_CeilDivideU32(group_count, SIMULATION_SCAN_ITEMS_PER_GROUP);
        if (parent_group_count > 1u)
        {
            Base_LogError("GPU count sort scan does not yet support more than two scan levels.");
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
            glUseProgram(0);
            return false;
        }

        glUniform1i(spatial_hash->scan_item_count_uniform, (i32) group_count);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, spatial_hash->group_sums_buffer.identifier);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spatial_hash->group_sums_scratch_buffer.identifier);
        OpenGL_DispatchCompute(1u, 1u, 1u);
        OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        glUseProgram(spatial_hash->scan_combine_program_identifier);
        glUniform1i(spatial_hash->scan_combine_item_count_uniform, (i32) item_count);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, elements_buffer->identifier);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spatial_hash->group_sums_buffer.identifier);
        OpenGL_DispatchCompute(group_count, 1, 1);
        OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glUseProgram(0);
    return true;
}

static bool SimulationSpatialHash_SortHashes (
    SimulationSpatialHash *spatial_hash,
    SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(spatial_hash != NULL);
    Base_Assert(particle_buffers != NULL);

    u32 particle_count = particle_buffers->particle_count;
    u32 workgroup_count_x = SimulationSpatialHash_CeilDivideU32(particle_count, 64u);

    glUseProgram(spatial_hash->reorder_hashes_program_identifier);
    glUniform1i(spatial_hash->reorder_hashes_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->spatial_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, spatial_hash->sorted_hash_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(spatial_hash->copyback_hashes_program_identifier);
    glUniform1i(spatial_hash->copyback_hashes_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spatial_hash->sorted_hash_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glUseProgram(0);
    return true;
}

static bool SimulationSpatialHash_BuildOffsets (
    SimulationSpatialHash *spatial_hash,
    SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(spatial_hash != NULL);
    Base_Assert(particle_buffers != NULL);

    u32 particle_count = particle_buffers->particle_count;
    u32 workgroup_count_x = SimulationSpatialHash_CeilDivideU32(particle_count, SIMULATION_SORT_THREAD_GROUP_SIZE);

    glUseProgram(spatial_hash->initialize_offsets_program_identifier);
    glUniform1i(spatial_hash->initialize_offsets_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_offset_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(spatial_hash->calculate_offsets_program_identifier);
    glUniform1i(spatial_hash->calculate_offsets_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->spatial_offset_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glUseProgram(0);
    return true;
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
    spatial_hash->clear_counts_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/count_sort_clear.comp");
    spatial_hash->calculate_counts_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/count_sort_counts.comp");
    spatial_hash->scatter_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/count_sort_scatter.comp");
    spatial_hash->copyback_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/count_sort_copyback.comp");
    spatial_hash->scan_block_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/scan_block.comp");
    spatial_hash->scan_combine_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/scan_combine.comp");
    spatial_hash->initialize_offsets_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/spatial_offsets_init.comp");
    spatial_hash->calculate_offsets_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/spatial_offsets_calculate.comp");
    spatial_hash->reorder_hashes_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/spatial_hash_reorder_hashes.comp");
    spatial_hash->copyback_hashes_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/spatial_hash_copyback_hashes.comp");

    bool initialize_success =
        spatial_hash->generate_keys_program_identifier != 0 &&
        spatial_hash->clear_counts_program_identifier != 0 &&
        spatial_hash->calculate_counts_program_identifier != 0 &&
        spatial_hash->scatter_program_identifier != 0 &&
        spatial_hash->copyback_program_identifier != 0 &&
        spatial_hash->scan_block_program_identifier != 0 &&
        spatial_hash->scan_combine_program_identifier != 0 &&
        spatial_hash->initialize_offsets_program_identifier != 0 &&
        spatial_hash->calculate_offsets_program_identifier != 0 &&
        spatial_hash->reorder_hashes_program_identifier != 0 &&
        spatial_hash->copyback_hashes_program_identifier != 0;

    if (!initialize_success)
    {
        Base_LogError("Failed to create one or more GPU spatial hash programs.");
        SimulationSpatialHash_Shutdown(spatial_hash);
        return false;
    }

    spatial_hash->cell_size_uniform = glGetUniformLocation(spatial_hash->generate_keys_program_identifier, "u_cell_size");
    spatial_hash->particle_count_uniform = glGetUniformLocation(spatial_hash->generate_keys_program_identifier, "u_particle_count");
    spatial_hash->table_size_uniform = glGetUniformLocation(spatial_hash->generate_keys_program_identifier, "u_table_size");
    spatial_hash->clear_counts_particle_count_uniform = glGetUniformLocation(spatial_hash->clear_counts_program_identifier, "u_particle_count");
    spatial_hash->calculate_counts_particle_count_uniform = glGetUniformLocation(spatial_hash->calculate_counts_program_identifier, "u_particle_count");
    spatial_hash->scatter_particle_count_uniform = glGetUniformLocation(spatial_hash->scatter_program_identifier, "u_particle_count");
    spatial_hash->copyback_particle_count_uniform = glGetUniformLocation(spatial_hash->copyback_program_identifier, "u_particle_count");
    spatial_hash->scan_item_count_uniform = glGetUniformLocation(spatial_hash->scan_block_program_identifier, "u_item_count");
    spatial_hash->scan_combine_item_count_uniform = glGetUniformLocation(spatial_hash->scan_combine_program_identifier, "u_item_count");
    spatial_hash->initialize_offsets_particle_count_uniform = glGetUniformLocation(spatial_hash->initialize_offsets_program_identifier, "u_particle_count");
    spatial_hash->calculate_offsets_particle_count_uniform = glGetUniformLocation(spatial_hash->calculate_offsets_program_identifier, "u_particle_count");
    spatial_hash->reorder_hashes_particle_count_uniform = glGetUniformLocation(spatial_hash->reorder_hashes_program_identifier, "u_particle_count");
    spatial_hash->copyback_hashes_particle_count_uniform = glGetUniformLocation(spatial_hash->copyback_hashes_program_identifier, "u_particle_count");
    return true;
}

void SimulationSpatialHash_Shutdown (SimulationSpatialHash *spatial_hash)
{
    if (spatial_hash == NULL) return;

    if (spatial_hash->generate_keys_program_identifier != 0) glDeleteProgram(spatial_hash->generate_keys_program_identifier);
    if (spatial_hash->clear_counts_program_identifier != 0) glDeleteProgram(spatial_hash->clear_counts_program_identifier);
    if (spatial_hash->calculate_counts_program_identifier != 0) glDeleteProgram(spatial_hash->calculate_counts_program_identifier);
    if (spatial_hash->scatter_program_identifier != 0) glDeleteProgram(spatial_hash->scatter_program_identifier);
    if (spatial_hash->copyback_program_identifier != 0) glDeleteProgram(spatial_hash->copyback_program_identifier);
    if (spatial_hash->scan_block_program_identifier != 0) glDeleteProgram(spatial_hash->scan_block_program_identifier);
    if (spatial_hash->scan_combine_program_identifier != 0) glDeleteProgram(spatial_hash->scan_combine_program_identifier);
    if (spatial_hash->initialize_offsets_program_identifier != 0) glDeleteProgram(spatial_hash->initialize_offsets_program_identifier);
    if (spatial_hash->calculate_offsets_program_identifier != 0) glDeleteProgram(spatial_hash->calculate_offsets_program_identifier);
    if (spatial_hash->reorder_hashes_program_identifier != 0) glDeleteProgram(spatial_hash->reorder_hashes_program_identifier);
    if (spatial_hash->copyback_hashes_program_identifier != 0) glDeleteProgram(spatial_hash->copyback_hashes_program_identifier);

    OpenGL_DestroyBuffer(&spatial_hash->sorted_key_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->sorted_index_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->sorted_hash_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->counts_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->group_sums_buffer);
    OpenGL_DestroyBuffer(&spatial_hash->group_sums_scratch_buffer);
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
    u32 sort_workgroup_count_x = SimulationSpatialHash_CeilDivideU32(particle_count, SIMULATION_SORT_THREAD_GROUP_SIZE);
    u32 key_workgroup_count_x = SimulationSpatialHash_CeilDivideU32(particle_count, 64u);

    if (!SimulationSpatialHash_EnsureTemporaryBuffers(spatial_hash, particle_count))
    {
        return false;
    }

    glUseProgram(spatial_hash->generate_keys_program_identifier);
    glUniform1f(spatial_hash->cell_size_uniform, settings.cell_size);
    glUniform1i(spatial_hash->particle_count_uniform, (i32) particle_count);
    glUniform1i(spatial_hash->table_size_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, particle_buffers->spatial_index_buffer.identifier);
    OpenGL_DispatchCompute(key_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(spatial_hash->clear_counts_program_identifier);
    glUniform1i(spatial_hash->clear_counts_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spatial_hash->counts_buffer.identifier);
    OpenGL_DispatchCompute(sort_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(spatial_hash->calculate_counts_program_identifier);
    glUniform1i(spatial_hash->calculate_counts_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, spatial_hash->counts_buffer.identifier);
    OpenGL_DispatchCompute(sort_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    if (!SimulationSpatialHash_RunExclusiveScan(spatial_hash, &spatial_hash->counts_buffer, particle_count))
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, 0);
        glUseProgram(0);
        return false;
    }

    glUseProgram(spatial_hash->scatter_program_identifier);
    glUniform1i(spatial_hash->scatter_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, spatial_hash->sorted_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, spatial_hash->sorted_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, spatial_hash->counts_buffer.identifier);
    OpenGL_DispatchCompute(sort_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(spatial_hash->copyback_program_identifier);
    glUniform1i(spatial_hash->copyback_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->spatial_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, spatial_hash->sorted_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, spatial_hash->sorted_key_buffer.identifier);
    OpenGL_DispatchCompute(sort_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    if (!SimulationSpatialHash_SortHashes(spatial_hash, particle_buffers))
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, 0);
        glUseProgram(0);
        return false;
    }

    if (!SimulationSpatialHash_BuildOffsets(spatial_hash, particle_buffers))
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, 0);
        glUseProgram(0);
        return false;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, 0);
    glUseProgram(0);
    return true;
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

            if ((sorted_hashes[sorted_index] % particle_count) != sorted_keys[sorted_index])
            {
                validation_success = false;
                break;
            }

            if (sorted_keys[sorted_index] < particle_count)
            {
                if (offsets[sorted_keys[sorted_index]] > sorted_index)
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
