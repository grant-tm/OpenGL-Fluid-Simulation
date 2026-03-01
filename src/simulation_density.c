#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_density.h"
#include "simulation_reorder.h"
#include "simulation_spatial_hash.h"

bool SimulationDensity_Initialize (SimulationDensity *density)
{
    Base_Assert(density != NULL);
    memset(density, 0, sizeof(*density));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    density->density_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/density.comp");
    if (density->density_program_identifier == 0)
    {
        Base_LogError("Failed to create the density compute program.");
        return false;
    }

    density->smoothing_radius_uniform = glGetUniformLocation(density->density_program_identifier, "u_smoothing_radius");
    density->particle_count_uniform = glGetUniformLocation(density->density_program_identifier, "u_particle_count");
    density->table_size_uniform = glGetUniformLocation(density->density_program_identifier, "u_table_size");
    return true;
}

void SimulationDensity_Shutdown (SimulationDensity *density)
{
    if (density == NULL) return;

    if (density->density_program_identifier != 0)
    {
        glDeleteProgram(density->density_program_identifier);
    }

    memset(density, 0, sizeof(*density));
}

bool SimulationDensity_Run (
    SimulationDensity *density,
    SimulationParticleBuffers *particle_buffers,
    SimulationDensitySettings settings)
{
    Base_Assert(density != NULL);
    Base_Assert(particle_buffers != NULL);

    if (density->density_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return false;
    if (settings.smoothing_radius <= 0.0f)
    {
        Base_LogError("Density smoothing radius must be greater than zero.");
        return false;
    }

    u32 particle_count = particle_buffers->particle_count;
    u32 workgroup_count_x = (particle_count + 63u) / 64u;

    glUseProgram(density->density_program_identifier);
    glUniform1f(density->smoothing_radius_uniform, settings.smoothing_radius);
    glUniform1i(density->particle_count_uniform, (i32) particle_count);
    glUniform1i(density->table_size_uniform, (i32) particle_count);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, particle_buffers->density_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particle_buffers->spatial_key_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, particle_buffers->spatial_hash_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, particle_buffers->spatial_offset_buffer.identifier);

    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_BUFFER_UPDATE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, 0);
    glUseProgram(0);
    return true;
}

bool SimulationDensity_RunValidation (void)
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

    SimulationSpatialHashSettings spatial_hash_settings = {0};
    spatial_hash_settings.cell_size = 1.5f;

    bool spatial_hash_success = SimulationSpatialHash_Run(&spatial_hash, &particle_buffers, spatial_hash_settings);
    bool reorder_success = spatial_hash_success && SimulationReorder_Run(&particle_buffers);

    SimulationDensity density = {0};
    if (!SimulationDensity_Initialize(&density))
    {
        SimulationSpatialHash_Shutdown(&spatial_hash);
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationDensitySettings density_settings = {0};
    density_settings.smoothing_radius = 1.5f;
    bool density_success = reorder_success && SimulationDensity_Run(&density, &particle_buffers, density_settings);

    Vec4 density_values[32];
    bool read_success = density_success &&
        OpenGL_ReadBuffer(&particle_buffers.density_buffer, density_values, (i32) sizeof(density_values));

    f32 minimum_density = 1000000.0f;
    f32 maximum_density = -1000000.0f;
    f32 minimum_near_density = 1000000.0f;
    f32 maximum_near_density = -1000000.0f;
    bool validation_success = read_success;

    if (validation_success)
    {
        for (u32 particle_index = 0; particle_index < particle_buffers.particle_count; particle_index++)
        {
            f32 density_value = density_values[particle_index].x;
            f32 near_density_value = density_values[particle_index].y;

            if (density_value < 0.0f || near_density_value < 0.0f)
            {
                validation_success = false;
                break;
            }

            if (density_value < minimum_density) minimum_density = density_value;
            if (density_value > maximum_density) maximum_density = density_value;
            if (near_density_value < minimum_near_density) minimum_near_density = near_density_value;
            if (near_density_value > maximum_near_density) maximum_near_density = near_density_value;
        }
    }

    if (validation_success)
    {
        validation_success =
            maximum_density > 0.0f &&
            maximum_near_density > 0.0f &&
            maximum_density > minimum_density + BASE_EPSILON32 &&
            maximum_near_density > minimum_near_density + BASE_EPSILON32;
    }

    SimulationDensity_Shutdown(&density);
    SimulationSpatialHash_Shutdown(&spatial_hash);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Density validation failed.");
        return false;
    }

    Base_LogInfo("Density validation passed.");
    return true;
}
