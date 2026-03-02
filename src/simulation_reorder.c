#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_reorder.h"

typedef struct SimulationReorderState
{
    bool is_initialized;
    u32 reorder_program_identifier;
    u32 copyback_program_identifier;
    i32 reorder_particle_count_uniform;
    i32 copyback_particle_count_uniform;
} SimulationReorderState;

static SimulationReorderState simulation_reorder_state = {0};

static bool SimulationReorder_EnsureInitialized (void)
{
    if (simulation_reorder_state.is_initialized)
    {
        return true;
    }

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    simulation_reorder_state.reorder_program_identifier =
        OpenGL_CreateComputeProgramFromFile("assets/shaders/particle_reorder.comp");
    simulation_reorder_state.copyback_program_identifier =
        OpenGL_CreateComputeProgramFromFile("assets/shaders/particle_reorder_copyback.comp");

    if (simulation_reorder_state.reorder_program_identifier == 0 ||
        simulation_reorder_state.copyback_program_identifier == 0)
    {
        Base_LogError("Failed to create GPU reorder compute programs.");
        if (simulation_reorder_state.reorder_program_identifier != 0)
        {
            glDeleteProgram(simulation_reorder_state.reorder_program_identifier);
        }
        if (simulation_reorder_state.copyback_program_identifier != 0)
        {
            glDeleteProgram(simulation_reorder_state.copyback_program_identifier);
        }
        memset(&simulation_reorder_state, 0, sizeof(simulation_reorder_state));
        return false;
    }

    simulation_reorder_state.reorder_particle_count_uniform =
        glGetUniformLocation(simulation_reorder_state.reorder_program_identifier, "u_particle_count");
    simulation_reorder_state.copyback_particle_count_uniform =
        glGetUniformLocation(simulation_reorder_state.copyback_program_identifier, "u_particle_count");
    simulation_reorder_state.is_initialized = true;
    return true;
}

bool SimulationReorder_Run (SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(particle_buffers != NULL);

    if (!SimulationReorder_EnsureInitialized())
    {
        return false;
    }

    u32 particle_count = particle_buffers->particle_count;
    if (particle_count == 0u) return false;

    u32 workgroup_count_x = (particle_count + 63u) / 64u;

    glUseProgram(simulation_reorder_state.reorder_program_identifier);
    glUniform1i(simulation_reorder_state.reorder_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->velocity_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, particle_buffers->spatial_index_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->sort_target_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->sort_target_predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, particle_buffers->sort_target_velocity_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(simulation_reorder_state.copyback_program_identifier);
    glUniform1i(simulation_reorder_state.copyback_particle_count_uniform, (i32) particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->velocity_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, particle_buffers->sort_target_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->sort_target_predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->sort_target_velocity_buffer.identifier);
    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

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

bool SimulationReorder_RunValidation (void)
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

    u32 custom_indices[32];
    for (u32 particle_index = 0; particle_index < particle_buffers.particle_count; particle_index++)
    {
        custom_indices[particle_index] = particle_buffers.particle_count - 1 - particle_index;
    }

    bool update_success = OpenGL_UpdateBuffer(
        &particle_buffers.spatial_index_buffer,
        0,
        (i32) sizeof(custom_indices),
        custom_indices);

    bool reorder_success = update_success && SimulationReorder_Run(&particle_buffers);

    Vec4 readback_positions[32];
    Vec4 readback_predicted_positions[32];
    Vec4 readback_velocities[32];

    bool read_success = reorder_success &&
        OpenGL_ReadBuffer(&particle_buffers.position_buffer, readback_positions, (i32) sizeof(readback_positions)) &&
        OpenGL_ReadBuffer(&particle_buffers.predicted_position_buffer, readback_predicted_positions, (i32) sizeof(readback_predicted_positions)) &&
        OpenGL_ReadBuffer(&particle_buffers.velocity_buffer, readback_velocities, (i32) sizeof(readback_velocities));

    bool validation_success = read_success;
    if (validation_success)
    {
        validation_success =
            Base_F32NearlyEqual(readback_positions[0].x, 1.5f, BASE_EPSILON32) &&
            Base_F32NearlyEqual(readback_positions[0].y, 1.5f, BASE_EPSILON32) &&
            Base_F32NearlyEqual(readback_positions[0].z, 0.5f, BASE_EPSILON32) &&
            Base_F32NearlyEqual(readback_positions[31].x, -1.5f, BASE_EPSILON32) &&
            Base_F32NearlyEqual(readback_positions[31].y, -1.5f, BASE_EPSILON32) &&
            Base_F32NearlyEqual(readback_positions[31].z, -0.5f, BASE_EPSILON32);
    }

    if (validation_success)
    {
        for (u32 particle_index = 0; particle_index < particle_buffers.particle_count; particle_index++)
        {
            validation_success =
                Base_F32NearlyEqual(readback_positions[particle_index].x, readback_predicted_positions[particle_index].x, BASE_EPSILON32) &&
                Base_F32NearlyEqual(readback_positions[particle_index].y, readback_predicted_positions[particle_index].y, BASE_EPSILON32) &&
                Base_F32NearlyEqual(readback_positions[particle_index].z, readback_predicted_positions[particle_index].z, BASE_EPSILON32) &&
                Base_F32NearlyEqual(readback_velocities[particle_index].x, 0.0f, BASE_EPSILON32) &&
                Base_F32NearlyEqual(readback_velocities[particle_index].y, 0.0f, BASE_EPSILON32) &&
                Base_F32NearlyEqual(readback_velocities[particle_index].z, 0.0f, BASE_EPSILON32);

            if (!validation_success) break;
        }
    }

    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Particle reorder validation failed.");
        return false;
    }

    Base_LogInfo("Particle reorder validation passed.");
    return true;
}
