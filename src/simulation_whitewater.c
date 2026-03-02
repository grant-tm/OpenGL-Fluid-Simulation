#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_reorder.h"
#include "simulation_spatial_hash.h"
#include "simulation_spawner.h"
#include "simulation_whitewater.h"

static bool SimulationWhitewater_CreateBuffers (SimulationWhitewater *whitewater, u32 maximum_particle_count)
{
    SimulationWhitewaterParticleGpu *initial_particle_data;
    u32 initial_counter_data[2] = {0};

    Base_Assert(whitewater != NULL);

    initial_particle_data = (SimulationWhitewaterParticleGpu *) calloc(
        maximum_particle_count,
        sizeof(SimulationWhitewaterParticleGpu));
    if (initial_particle_data == NULL)
    {
        Base_LogError("Failed to allocate initial whitewater particle data.");
        return false;
    }

    bool success =
        OpenGL_CreateBuffer(
            &whitewater->particle_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (maximum_particle_count * sizeof(SimulationWhitewaterParticleGpu)),
            initial_particle_data,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &whitewater->compacted_particle_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (maximum_particle_count * sizeof(SimulationWhitewaterParticleGpu)),
            initial_particle_data,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &whitewater->counter_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) sizeof(initial_counter_data),
            initial_counter_data,
            GL_DYNAMIC_COPY);

    free(initial_particle_data);
    return success;
}

bool SimulationWhitewater_Initialize (SimulationWhitewater *whitewater, u32 maximum_particle_count)
{
    Base_Assert(whitewater != NULL);
    memset(whitewater, 0, sizeof(*whitewater));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    if (maximum_particle_count == 0)
    {
        Base_LogError("Whitewater maximum particle count must be greater than zero.");
        return false;
    }

    whitewater->maximum_particle_count = maximum_particle_count;

    if (!SimulationWhitewater_CreateBuffers(whitewater, maximum_particle_count))
    {
        SimulationWhitewater_Shutdown(whitewater);
        Base_LogError("Failed to create whitewater buffers.");
        return false;
    }

    whitewater->spawn_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/whitewater_spawn.comp");
    whitewater->update_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/whitewater_update.comp");
    whitewater->copyback_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/whitewater_copyback.comp");

    if (whitewater->spawn_program_identifier == 0 ||
        whitewater->update_program_identifier == 0 ||
        whitewater->copyback_program_identifier == 0)
    {
        SimulationWhitewater_Shutdown(whitewater);
        Base_LogError("Failed to create whitewater compute programs.");
        return false;
    }

    whitewater->spawn_particle_count_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_particle_count");
    whitewater->spawn_table_size_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_table_size");
    whitewater->spawn_maximum_whitewater_count_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_maximum_whitewater_count");
    whitewater->spawn_delta_time_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_delta_time");
    whitewater->spawn_simulation_time_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_simulation_time");
    whitewater->spawn_spawn_rate_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_spawn_rate");
    whitewater->spawn_trapped_air_velocity_minimum_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_trapped_air_velocity_minimum");
    whitewater->spawn_trapped_air_velocity_maximum_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_trapped_air_velocity_maximum");
    whitewater->spawn_kinetic_energy_minimum_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_kinetic_energy_minimum");
    whitewater->spawn_kinetic_energy_maximum_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_kinetic_energy_maximum");
    whitewater->spawn_bubble_scale_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_bubble_scale");
    whitewater->spawn_target_density_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_target_density");
    whitewater->spawn_smoothing_radius_uniform =
        glGetUniformLocation(whitewater->spawn_program_identifier, "u_smoothing_radius");

    whitewater->update_particle_count_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_particle_count");
    whitewater->update_table_size_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_table_size");
    whitewater->update_maximum_whitewater_count_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_maximum_whitewater_count");
    whitewater->update_delta_time_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_delta_time");
    whitewater->update_gravity_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_gravity");
    whitewater->update_bubble_buoyancy_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_bubble_buoyancy");
    whitewater->update_smoothing_radius_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_smoothing_radius");
    whitewater->update_spray_classify_maximum_neighbors_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_spray_classify_maximum_neighbors");
    whitewater->update_bubble_classify_minimum_neighbors_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_bubble_classify_minimum_neighbors");
    whitewater->update_bubble_scale_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_bubble_scale");
    whitewater->update_bubble_scale_change_speed_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_bubble_scale_change_speed");
    whitewater->update_collision_damping_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_collision_damping");
    whitewater->update_bounds_size_uniform =
        glGetUniformLocation(whitewater->update_program_identifier, "u_bounds_size");

    whitewater->copyback_maximum_whitewater_count_uniform =
        glGetUniformLocation(whitewater->copyback_program_identifier, "u_maximum_whitewater_count");

    return SimulationWhitewater_Reset(whitewater);
}

void SimulationWhitewater_Shutdown (SimulationWhitewater *whitewater)
{
    if (whitewater == NULL) return;

    if (whitewater->spawn_program_identifier != 0) glDeleteProgram(whitewater->spawn_program_identifier);
    if (whitewater->update_program_identifier != 0) glDeleteProgram(whitewater->update_program_identifier);
    if (whitewater->copyback_program_identifier != 0) glDeleteProgram(whitewater->copyback_program_identifier);
    OpenGL_DestroyBuffer(&whitewater->particle_buffer);
    OpenGL_DestroyBuffer(&whitewater->compacted_particle_buffer);
    OpenGL_DestroyBuffer(&whitewater->counter_buffer);
    memset(whitewater, 0, sizeof(*whitewater));
}

bool SimulationWhitewater_Reset (SimulationWhitewater *whitewater)
{
    SimulationWhitewaterParticleGpu *empty_particle_data;
    u32 counter_data[2] = {0};

    Base_Assert(whitewater != NULL);

    if (whitewater->maximum_particle_count == 0) return false;

    empty_particle_data = (SimulationWhitewaterParticleGpu *) calloc(
        whitewater->maximum_particle_count,
        sizeof(SimulationWhitewaterParticleGpu));
    if (empty_particle_data == NULL)
    {
        Base_LogError("Failed to allocate whitewater reset data.");
        return false;
    }

    bool success =
        OpenGL_UpdateBuffer(
            &whitewater->particle_buffer,
            0,
            (i32) (whitewater->maximum_particle_count * sizeof(SimulationWhitewaterParticleGpu)),
            empty_particle_data) &&
        OpenGL_UpdateBuffer(
            &whitewater->compacted_particle_buffer,
            0,
            (i32) (whitewater->maximum_particle_count * sizeof(SimulationWhitewaterParticleGpu)),
            empty_particle_data) &&
        OpenGL_UpdateBuffer(&whitewater->counter_buffer, 0, (i32) sizeof(counter_data), counter_data);

    free(empty_particle_data);
    return success;
}

bool SimulationWhitewater_Run (
    SimulationWhitewater *whitewater,
    const SimulationParticleBuffers *particle_buffers,
    SimulationWhitewaterSettings settings)
{
    u32 particle_workgroup_count_x;
    u32 whitewater_workgroup_count_x;

    Base_Assert(whitewater != NULL);
    Base_Assert(particle_buffers != NULL);

    if (whitewater->spawn_program_identifier == 0 ||
        whitewater->update_program_identifier == 0 ||
        whitewater->copyback_program_identifier == 0)
    {
        return false;
    }

    if (particle_buffers->particle_count == 0 || whitewater->maximum_particle_count == 0)
    {
        return true;
    }

    if (settings.smoothing_radius <= 0.0f || settings.delta_time_seconds < 0.0f)
    {
        Base_LogError("Whitewater settings are invalid.");
        return false;
    }

    particle_workgroup_count_x = (particle_buffers->particle_count + 63u) / 64u;
    whitewater_workgroup_count_x = (whitewater->maximum_particle_count + 63u) / 64u;

    glUseProgram(whitewater->spawn_program_identifier);
    glUniform1i(whitewater->spawn_particle_count_uniform, (i32) particle_buffers->particle_count);
    glUniform1i(whitewater->spawn_table_size_uniform, (i32) particle_buffers->particle_count);
    glUniform1i(whitewater->spawn_maximum_whitewater_count_uniform, (i32) whitewater->maximum_particle_count);
    glUniform1f(whitewater->spawn_delta_time_uniform, settings.delta_time_seconds);
    glUniform1f(whitewater->spawn_simulation_time_uniform, settings.simulation_time_seconds);
    glUniform1f(whitewater->spawn_spawn_rate_uniform, settings.spawn_rate);
    glUniform1f(whitewater->spawn_trapped_air_velocity_minimum_uniform, settings.trapped_air_velocity_minimum);
    glUniform1f(whitewater->spawn_trapped_air_velocity_maximum_uniform, settings.trapped_air_velocity_maximum);
    glUniform1f(whitewater->spawn_kinetic_energy_minimum_uniform, settings.kinetic_energy_minimum);
    glUniform1f(whitewater->spawn_kinetic_energy_maximum_uniform, settings.kinetic_energy_maximum);
    glUniform1f(whitewater->spawn_bubble_scale_uniform, settings.bubble_scale);
    glUniform1f(whitewater->spawn_target_density_uniform, settings.target_density);
    glUniform1f(whitewater->spawn_smoothing_radius_uniform, settings.smoothing_radius);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->velocity_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->density_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->spatial_offset_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, whitewater->particle_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, whitewater->counter_buffer.identifier);
    OpenGL_DispatchCompute(particle_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    return SimulationWhitewater_UpdateOnly(whitewater, particle_buffers, settings);
}

bool SimulationWhitewater_UpdateOnly (
    SimulationWhitewater *whitewater,
    const SimulationParticleBuffers *particle_buffers,
    SimulationWhitewaterSettings settings)
{
    u32 whitewater_workgroup_count_x;

    Base_Assert(whitewater != NULL);
    Base_Assert(particle_buffers != NULL);

    if (whitewater->update_program_identifier == 0 ||
        whitewater->copyback_program_identifier == 0)
    {
        return false;
    }

    if (particle_buffers->particle_count == 0 || whitewater->maximum_particle_count == 0)
    {
        return true;
    }

    if (settings.smoothing_radius <= 0.0f || settings.delta_time_seconds < 0.0f)
    {
        Base_LogError("Whitewater settings are invalid.");
        return false;
    }

    whitewater_workgroup_count_x = (whitewater->maximum_particle_count + 63u) / 64u;

    glUseProgram(whitewater->update_program_identifier);
    glUniform1i(whitewater->update_particle_count_uniform, (i32) particle_buffers->particle_count);
    glUniform1i(whitewater->update_table_size_uniform, (i32) particle_buffers->particle_count);
    glUniform1i(whitewater->update_maximum_whitewater_count_uniform, (i32) whitewater->maximum_particle_count);
    glUniform1f(whitewater->update_delta_time_uniform, settings.delta_time_seconds);
    glUniform1f(whitewater->update_gravity_uniform, settings.gravity);
    glUniform1f(whitewater->update_bubble_buoyancy_uniform, settings.bubble_buoyancy);
    glUniform1f(whitewater->update_smoothing_radius_uniform, settings.smoothing_radius);
    glUniform1i(whitewater->update_spray_classify_maximum_neighbors_uniform, settings.spray_classify_maximum_neighbors);
    glUniform1i(whitewater->update_bubble_classify_minimum_neighbors_uniform, settings.bubble_classify_minimum_neighbors);
    glUniform1f(whitewater->update_bubble_scale_uniform, settings.bubble_scale);
    glUniform1f(whitewater->update_bubble_scale_change_speed_uniform, settings.bubble_scale_change_speed);
    glUniform1f(whitewater->update_collision_damping_uniform, settings.collision_damping);
    glUniform3f(whitewater->update_bounds_size_uniform, settings.bounds_size.x, settings.bounds_size.y, settings.bounds_size.z);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->velocity_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_offset_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, whitewater->particle_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, whitewater->compacted_particle_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, whitewater->counter_buffer.identifier);
    OpenGL_DispatchCompute(whitewater_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glUseProgram(whitewater->copyback_program_identifier);
    glUniform1i(whitewater->copyback_maximum_whitewater_count_uniform, (i32) whitewater->maximum_particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, whitewater->particle_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, whitewater->compacted_particle_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, whitewater->counter_buffer.identifier);
    OpenGL_DispatchCompute(whitewater_workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, 0);
    glUseProgram(0);
    return true;
}

bool SimulationWhitewater_ReadActiveParticleCount (SimulationWhitewater *whitewater, u32 *active_particle_count)
{
    u32 counter_values[2];

    Base_Assert(whitewater != NULL);
    Base_Assert(active_particle_count != NULL);

    if (!OpenGL_ReadBuffer(&whitewater->counter_buffer, counter_values, (i32) sizeof(counter_values)))
    {
        return false;
    }

    *active_particle_count = counter_values[0];
    return true;
}

bool SimulationWhitewater_RunValidation (void)
{
    SimulationSpawnBox spawn_box = {0};
    SimulationSpawnData spawn_data = {0};
    SimulationParticleBuffers particle_buffers = {0};
    SimulationSpatialHash spatial_hash = {0};
    SimulationWhitewater whitewater = {0};
    SimulationSpatialHashSettings spatial_hash_settings = {0};
    SimulationWhitewaterSettings whitewater_settings = {0};
    u32 active_particle_count = 0;

    spawn_box.center = Vec3_Create(0.0f, 0.0f, 0.0f);
    spawn_box.size = Vec3_Create(3.0f, 2.0f, 2.0f);
    spawn_box.particle_spacing = 0.5f;
    spawn_box.initial_velocity = Vec3_Create(0.0f, 0.0f, 0.0f);

    if (!Simulation_GenerateSpawnDataBox(&spawn_data, spawn_box))
    {
        return false;
    }

    if (!Simulation_CreateParticleBuffersFromSpawnData(&particle_buffers, &spawn_data))
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    Vec4 *velocity_values = (Vec4 *) calloc(particle_buffers.particle_count, sizeof(Vec4));
    if (velocity_values == NULL)
    {
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        Base_LogError("Failed to allocate whitewater validation velocity data.");
        return false;
    }

    for (u32 particle_index = 0; particle_index < particle_buffers.particle_count; particle_index++)
    {
        f32 direction = (particle_index % 2u == 0u) ? 1.0f : -1.0f;
        velocity_values[particle_index] = Vec4_Create(3.0f * direction, 6.0f, 0.0f, 0.0f);
    }

    bool velocity_update_success = OpenGL_UpdateBuffer(
        &particle_buffers.velocity_buffer,
        0,
        (i32) (particle_buffers.particle_count * sizeof(Vec4)),
        velocity_values);
    free(velocity_values);

    if (!velocity_update_success)
    {
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        Base_LogError("Failed to upload whitewater validation velocity data.");
        return false;
    }

    if (!SimulationSpatialHash_Initialize(&spatial_hash) ||
        !SimulationWhitewater_Initialize(&whitewater, 512u))
    {
        SimulationWhitewater_Shutdown(&whitewater);
        SimulationSpatialHash_Shutdown(&spatial_hash);
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    spatial_hash_settings.cell_size = 0.5f;
    whitewater_settings.maximum_particle_count = 512u;
    whitewater_settings.spawn_rate = 180.0f;
    whitewater_settings.trapped_air_velocity_minimum = 1.0f;
    whitewater_settings.trapped_air_velocity_maximum = 10.0f;
    whitewater_settings.kinetic_energy_minimum = 1.0f;
    whitewater_settings.kinetic_energy_maximum = 50.0f;
    whitewater_settings.target_density = 6.0f;
    whitewater_settings.smoothing_radius = 0.5f;
    whitewater_settings.gravity = -9.8f;
    whitewater_settings.delta_time_seconds = 1.0f / 30.0f;
    whitewater_settings.bubble_buoyancy = 1.4f;
    whitewater_settings.spray_classify_maximum_neighbors = 5;
    whitewater_settings.bubble_classify_minimum_neighbors = 15;
    whitewater_settings.bubble_scale = 0.3f;
    whitewater_settings.bubble_scale_change_speed = 7.0f;
    whitewater_settings.collision_damping = 0.1f;
    whitewater_settings.bounds_size = Vec3_Create(4.0f, 4.0f, 4.0f);
    whitewater_settings.simulation_time_seconds = 0.5f;

    bool success =
        SimulationSpatialHash_Run(&spatial_hash, &particle_buffers, spatial_hash_settings) &&
        SimulationReorder_Run(&particle_buffers) &&
        SimulationWhitewater_Run(&whitewater, &particle_buffers, whitewater_settings) &&
        SimulationWhitewater_ReadActiveParticleCount(&whitewater, &active_particle_count);

    SimulationWhitewater_Shutdown(&whitewater);
    SimulationSpatialHash_Shutdown(&spatial_hash);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!success || active_particle_count > 512u)
    {
        Base_LogError("Whitewater validation failed.");
        return false;
    }

    Base_LogInfo("Whitewater validation passed.");
    return true;
}
