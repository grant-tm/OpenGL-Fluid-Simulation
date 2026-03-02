#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "opengl_helpers.h"
#include "simulation_density.h"
#include "simulation_pressure.h"
#include "simulation_reorder.h"
#include "simulation_spatial_hash.h"

bool SimulationPressure_Initialize (SimulationPressure *pressure)
{
    Base_Assert(pressure != NULL);
    memset(pressure, 0, sizeof(*pressure));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    pressure->pressure_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/pressure.comp");
    if (pressure->pressure_program_identifier == 0)
    {
        Base_LogError("Failed to create the pressure compute program.");
        return false;
    }

    pressure->smoothing_radius_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_smoothing_radius");
    pressure->particle_count_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_particle_count");
    pressure->table_size_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_table_size");
    pressure->delta_time_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_delta_time");
    pressure->target_density_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_target_density");
    pressure->pressure_multiplier_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_pressure_multiplier");
    pressure->near_pressure_multiplier_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_near_pressure_multiplier");
    pressure->enable_whitewater_spawn_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_enable_whitewater_spawn");
    pressure->maximum_whitewater_count_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_maximum_whitewater_count");
    pressure->simulation_time_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_simulation_time");
    pressure->spawn_rate_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_spawn_rate");
    pressure->spawn_rate_fade_in_time_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_spawn_rate_fade_in_time");
    pressure->spawn_rate_fade_start_time_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_spawn_rate_fade_start_time");
    pressure->trapped_air_velocity_minimum_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_trapped_air_velocity_minimum");
    pressure->trapped_air_velocity_maximum_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_trapped_air_velocity_maximum");
    pressure->kinetic_energy_minimum_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_kinetic_energy_minimum");
    pressure->kinetic_energy_maximum_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_kinetic_energy_maximum");
    pressure->bubble_scale_uniform = glGetUniformLocation(pressure->pressure_program_identifier, "u_bubble_scale");
    return true;
}

void SimulationPressure_Shutdown (SimulationPressure *pressure)
{
    if (pressure == NULL) return;

    if (pressure->pressure_program_identifier != 0)
    {
        glDeleteProgram(pressure->pressure_program_identifier);
    }

    memset(pressure, 0, sizeof(*pressure));
}

bool SimulationPressure_Run (
    SimulationPressure *pressure,
    SimulationParticleBuffers *particle_buffers,
    SimulationPressureSettings settings,
    f32 delta_time_seconds,
    SimulationWhitewater *whitewater,
    const SimulationWhitewaterSettings *whitewater_settings)
{
    Base_Assert(pressure != NULL);
    Base_Assert(particle_buffers != NULL);

    if (pressure->pressure_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return false;
    if (settings.smoothing_radius <= 0.0f)
    {
        Base_LogError("Pressure smoothing radius must be greater than zero.");
        return false;
    }

    u32 particle_count = particle_buffers->particle_count;
    u32 workgroup_count_x = (particle_count + 63u) / 64u;

    glUseProgram(pressure->pressure_program_identifier);
    glUniform1f(pressure->smoothing_radius_uniform, settings.smoothing_radius);
    glUniform1i(pressure->particle_count_uniform, (i32) particle_count);
    glUniform1i(pressure->table_size_uniform, (i32) particle_count);
    glUniform1f(pressure->delta_time_uniform, delta_time_seconds);
    glUniform1f(pressure->target_density_uniform, settings.target_density);
    glUniform1f(pressure->pressure_multiplier_uniform, settings.pressure_multiplier);
    glUniform1f(pressure->near_pressure_multiplier_uniform, settings.near_pressure_multiplier);
    glUniform1i(
        pressure->enable_whitewater_spawn_uniform,
        (whitewater != NULL && whitewater_settings != NULL && whitewater->maximum_particle_count > 0) ? 1 : 0);

    if (whitewater != NULL && whitewater_settings != NULL && whitewater->maximum_particle_count > 0)
    {
        glUniform1i(pressure->maximum_whitewater_count_uniform, (i32) whitewater->maximum_particle_count);
        glUniform1f(pressure->simulation_time_uniform, whitewater_settings->simulation_time_seconds);
        glUniform1f(pressure->spawn_rate_uniform, whitewater_settings->spawn_rate);
        glUniform1f(pressure->spawn_rate_fade_in_time_uniform, whitewater_settings->spawn_rate_fade_in_time);
        glUniform1f(pressure->spawn_rate_fade_start_time_uniform, whitewater_settings->spawn_rate_fade_start_time);
        glUniform1f(pressure->trapped_air_velocity_minimum_uniform, whitewater_settings->trapped_air_velocity_minimum);
        glUniform1f(pressure->trapped_air_velocity_maximum_uniform, whitewater_settings->trapped_air_velocity_maximum);
        glUniform1f(pressure->kinetic_energy_minimum_uniform, whitewater_settings->kinetic_energy_minimum);
        glUniform1f(pressure->kinetic_energy_maximum_uniform, whitewater_settings->kinetic_energy_maximum);
        glUniform1f(pressure->bubble_scale_uniform, whitewater_settings->bubble_scale);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->velocity_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, particle_buffers->density_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, particle_buffers->spatial_offset_buffer.identifier);
    if (whitewater != NULL && whitewater_settings != NULL && whitewater->maximum_particle_count > 0)
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, whitewater->particle_buffer.identifier);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, whitewater->counter_buffer.identifier);
    }

    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_BUFFER_UPDATE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, 0);
    glUseProgram(0);
    return true;
}

bool SimulationPressure_RunValidation (void)
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
    SimulationDensity density = {0};
    SimulationPressure pressure = {0};

    bool success =
        SimulationSpatialHash_Initialize(&spatial_hash) &&
        SimulationDensity_Initialize(&density) &&
        SimulationPressure_Initialize(&pressure);

    if (!success)
    {
        SimulationPressure_Shutdown(&pressure);
        SimulationDensity_Shutdown(&density);
        SimulationSpatialHash_Shutdown(&spatial_hash);
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationSpatialHashSettings spatial_hash_settings = {0};
    spatial_hash_settings.cell_size = 1.5f;

    SimulationDensitySettings density_settings = {0};
    density_settings.smoothing_radius = 1.5f;

    SimulationPressureSettings pressure_settings = {0};
    pressure_settings.smoothing_radius = 1.5f;
    pressure_settings.target_density = 1.0f;
    pressure_settings.pressure_multiplier = 6.0f;
    pressure_settings.near_pressure_multiplier = 2.0f;

    success =
        SimulationSpatialHash_Run(&spatial_hash, &particle_buffers, spatial_hash_settings) &&
        SimulationReorder_Run(&particle_buffers) &&
        SimulationDensity_Run(&density, &particle_buffers, density_settings) &&
        SimulationPressure_Run(&pressure, &particle_buffers, pressure_settings, 1.0f / 60.0f, NULL, NULL);

    Vec4 velocity_values[32];
    bool read_success = success &&
        OpenGL_ReadBuffer(&particle_buffers.velocity_buffer, velocity_values, (i32) sizeof(velocity_values));

    bool validation_success = read_success;
    bool found_nontrivial_velocity = false;

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

            f32 velocity_length_squared = velocity_x * velocity_x + velocity_y * velocity_y + velocity_z * velocity_z;
            if (velocity_length_squared > 0.000001f)
            {
                found_nontrivial_velocity = true;
            }
        }
    }

    validation_success = validation_success && found_nontrivial_velocity;

    SimulationPressure_Shutdown(&pressure);
    SimulationDensity_Shutdown(&density);
    SimulationSpatialHash_Shutdown(&spatial_hash);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Pressure validation failed.");
        return false;
    }

    Base_LogInfo("Pressure validation passed.");
    return true;
}
