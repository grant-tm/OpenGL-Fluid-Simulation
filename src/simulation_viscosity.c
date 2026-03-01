#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_reorder.h"
#include "simulation_spatial_hash.h"
#include "simulation_viscosity.h"

bool SimulationViscosity_Initialize (SimulationViscosity *viscosity)
{
    Base_Assert(viscosity != NULL);
    memset(viscosity, 0, sizeof(*viscosity));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    viscosity->viscosity_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/viscosity.comp");
    if (viscosity->viscosity_program_identifier == 0)
    {
        Base_LogError("Failed to create the viscosity compute program.");
        return false;
    }

    viscosity->smoothing_radius_uniform = glGetUniformLocation(viscosity->viscosity_program_identifier, "u_smoothing_radius");
    viscosity->particle_count_uniform = glGetUniformLocation(viscosity->viscosity_program_identifier, "u_particle_count");
    viscosity->table_size_uniform = glGetUniformLocation(viscosity->viscosity_program_identifier, "u_table_size");
    viscosity->delta_time_uniform = glGetUniformLocation(viscosity->viscosity_program_identifier, "u_delta_time");
    viscosity->viscosity_strength_uniform = glGetUniformLocation(viscosity->viscosity_program_identifier, "u_viscosity_strength");
    return true;
}

void SimulationViscosity_Shutdown (SimulationViscosity *viscosity)
{
    if (viscosity == NULL) return;

    if (viscosity->viscosity_program_identifier != 0)
    {
        glDeleteProgram(viscosity->viscosity_program_identifier);
    }

    memset(viscosity, 0, sizeof(*viscosity));
}

bool SimulationViscosity_Run (
    SimulationViscosity *viscosity,
    SimulationParticleBuffers *particle_buffers,
    SimulationViscositySettings settings,
    f32 delta_time_seconds)
{
    Base_Assert(viscosity != NULL);
    Base_Assert(particle_buffers != NULL);

    if (viscosity->viscosity_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return false;
    if (settings.smoothing_radius <= 0.0f)
    {
        Base_LogError("Viscosity smoothing radius must be greater than zero.");
        return false;
    }
    if (settings.viscosity_strength == 0.0f)
    {
        return true;
    }

    u32 particle_count = particle_buffers->particle_count;
    u32 workgroup_count_x = (particle_count + 63u) / 64u;

    glUseProgram(viscosity->viscosity_program_identifier);
    glUniform1f(viscosity->smoothing_radius_uniform, settings.smoothing_radius);
    glUniform1i(viscosity->particle_count_uniform, (i32) particle_count);
    glUniform1i(viscosity->table_size_uniform, (i32) particle_count);
    glUniform1f(viscosity->delta_time_uniform, delta_time_seconds);
    glUniform1f(viscosity->viscosity_strength_uniform, settings.viscosity_strength);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->velocity_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, particle_buffers->spatial_offset_buffer.identifier);

    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, 0);
    glUseProgram(0);
    return true;
}

bool SimulationViscosity_RunValidation (void)
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

    for (u32 particle_index = 0; particle_index < spawn_data.particle_count; particle_index++)
    {
        f32 direction = (particle_index % 2 == 0) ? 1.0f : -1.0f;
        spawn_data.velocities[particle_index] = Vec4_Create(direction * 2.0f, 0.0f, 0.0f, 0.0f);
    }

    SimulationParticleBuffers particle_buffers = {0};
    if (!Simulation_CreateParticleBuffersFromSpawnData(&particle_buffers, &spawn_data))
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationSpatialHash spatial_hash = {0};
    SimulationViscosity viscosity = {0};

    bool success =
        SimulationSpatialHash_Initialize(&spatial_hash) &&
        SimulationViscosity_Initialize(&viscosity);

    if (!success)
    {
        SimulationViscosity_Shutdown(&viscosity);
        SimulationSpatialHash_Shutdown(&spatial_hash);
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationSpatialHashSettings spatial_hash_settings = {0};
    spatial_hash_settings.cell_size = 1.5f;

    SimulationViscositySettings viscosity_settings = {0};
    viscosity_settings.smoothing_radius = 1.5f;
    viscosity_settings.viscosity_strength = 0.35f;

    success =
        SimulationSpatialHash_Run(&spatial_hash, &particle_buffers, spatial_hash_settings) &&
        SimulationReorder_Run(&particle_buffers) &&
        SimulationViscosity_Run(&viscosity, &particle_buffers, viscosity_settings, 1.0f / 60.0f);

    Vec4 velocity_values[32];
    bool read_success = success &&
        OpenGL_ReadBuffer(&particle_buffers.velocity_buffer, velocity_values, (i32) sizeof(velocity_values));

    bool validation_success = read_success;
    bool found_changed_velocity = false;

    if (validation_success)
    {
        for (u32 particle_index = 0; particle_index < particle_buffers.particle_count; particle_index++)
        {
            f32 velocity_x = velocity_values[particle_index].x;
            f32 velocity_y = velocity_values[particle_index].y;
            f32 velocity_z = velocity_values[particle_index].z;

            if (!_finite(velocity_x) || !_finite(velocity_y) || !_finite(velocity_z))
            {
                validation_success = false;
                break;
            }

            if (fabsf(fabsf(velocity_x) - 2.0f) > 0.0001f)
            {
                found_changed_velocity = true;
            }
        }
    }

    validation_success = validation_success && found_changed_velocity;

    SimulationViscosity_Shutdown(&viscosity);
    SimulationSpatialHash_Shutdown(&spatial_hash);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Viscosity validation failed.");
        return false;
    }

    Base_LogInfo("Viscosity validation passed.");
    return true;
}
