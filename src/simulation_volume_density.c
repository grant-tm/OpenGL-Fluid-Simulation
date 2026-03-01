#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_spawner.h"
#include "simulation_volume_density.h"

static bool SimulationVolumeDensity_RecreateTextureIfNeeded (
    SimulationVolumeDensity *volume_density,
    SimulationVolumeDensitySettings settings)
{
    Base_Assert(volume_density != NULL);

    bool dimensions_match =
        volume_density->density_texture.identifier != 0 &&
        volume_density->density_texture.width == settings.resolution_x &&
        volume_density->density_texture.height == settings.resolution_y &&
        volume_density->density_texture.depth == settings.resolution_z;

    if (dimensions_match)
    {
        return true;
    }

    OpenGL_DestroyTexture3D(&volume_density->density_texture);
    return OpenGL_CreateTexture3D(
        &volume_density->density_texture,
        settings.resolution_x,
        settings.resolution_y,
        settings.resolution_z,
        GL_R32F,
        GL_RED,
        GL_FLOAT,
        NULL);
}

bool SimulationVolumeDensity_Initialize (
    SimulationVolumeDensity *volume_density,
    SimulationVolumeDensitySettings settings)
{
    Base_Assert(volume_density != NULL);
    memset(volume_density, 0, sizeof(*volume_density));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    if (!OpenGL_LoadImageFunctions())
    {
        return false;
    }

    if (settings.resolution_x <= 0 || settings.resolution_y <= 0 || settings.resolution_z <= 0)
    {
        Base_LogError("Volume density resolution must be positive.");
        return false;
    }

    if (!OpenGL_CreateTexture3D(
        &volume_density->density_texture,
        settings.resolution_x,
        settings.resolution_y,
        settings.resolution_z,
        GL_R32F,
        GL_RED,
        GL_FLOAT,
        NULL))
    {
        return false;
    }

    volume_density->density_volume_program_identifier =
        OpenGL_CreateComputeProgramFromFile("assets/shaders/density_volume.comp");
    if (volume_density->density_volume_program_identifier == 0)
    {
        OpenGL_DestroyTexture3D(&volume_density->density_texture);
        Base_LogError("Failed to create the density volume compute program.");
        return false;
    }

    volume_density->particle_count_uniform =
        glGetUniformLocation(volume_density->density_volume_program_identifier, "u_particle_count");
    volume_density->bounds_size_uniform =
        glGetUniformLocation(volume_density->density_volume_program_identifier, "u_bounds_size");
    volume_density->resolution_uniform =
        glGetUniformLocation(volume_density->density_volume_program_identifier, "u_resolution");
    volume_density->smoothing_radius_uniform =
        glGetUniformLocation(volume_density->density_volume_program_identifier, "u_smoothing_radius");
    volume_density->density_scale_uniform =
        glGetUniformLocation(volume_density->density_volume_program_identifier, "u_density_scale");

    return true;
}

void SimulationVolumeDensity_Shutdown (SimulationVolumeDensity *volume_density)
{
    if (volume_density == NULL) return;

    if (volume_density->density_volume_program_identifier != 0)
    {
        glDeleteProgram(volume_density->density_volume_program_identifier);
    }

    OpenGL_DestroyTexture3D(&volume_density->density_texture);
    memset(volume_density, 0, sizeof(*volume_density));
}

bool SimulationVolumeDensity_Run (
    SimulationVolumeDensity *volume_density,
    const SimulationParticleBuffers *particle_buffers,
    SimulationVolumeDensitySettings settings)
{
    Base_Assert(volume_density != NULL);
    Base_Assert(particle_buffers != NULL);

    if (volume_density->density_volume_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return false;
    if (settings.smoothing_radius <= 0.0f)
    {
        Base_LogError("Volume density smoothing radius must be greater than zero.");
        return false;
    }

    if (!SimulationVolumeDensity_RecreateTextureIfNeeded(volume_density, settings))
    {
        return false;
    }

    f32 clear_value = 0.0f;
    if (!OpenGL_ClearTexture3D(&volume_density->density_texture, GL_RED, GL_FLOAT, &clear_value))
    {
        return false;
    }

    u32 group_count_x = ((u32) settings.resolution_x + 3u) / 4u;
    u32 group_count_y = ((u32) settings.resolution_y + 3u) / 4u;
    u32 group_count_z = ((u32) settings.resolution_z + 3u) / 4u;

    glUseProgram(volume_density->density_volume_program_identifier);
    glUniform1i(volume_density->particle_count_uniform, (i32) particle_buffers->particle_count);
    glUniform3f(volume_density->bounds_size_uniform, settings.bounds_size.x, settings.bounds_size.y, settings.bounds_size.z);
    glUniform3i(volume_density->resolution_uniform, settings.resolution_x, settings.resolution_y, settings.resolution_z);
    glUniform1f(volume_density->smoothing_radius_uniform, settings.smoothing_radius);
    glUniform1f(volume_density->density_scale_uniform, settings.density_scale);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    OpenGL_BindImageTexture(0, volume_density->density_texture.identifier, 0, true, 0, GL_WRITE_ONLY, GL_R32F);
    OpenGL_DispatchCompute(group_count_x, group_count_y, group_count_z);
    OpenGL_MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    OpenGL_BindImageTexture(0, 0, 0, true, 0, GL_WRITE_ONLY, GL_R32F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glUseProgram(0);

    return true;
}

bool SimulationVolumeDensity_ReadbackSummary (
    SimulationVolumeDensity *volume_density,
    SimulationVolumeDensityReadbackSummary *summary)
{
    Base_Assert(volume_density != NULL);
    Base_Assert(summary != NULL);

    i32 texel_count =
        volume_density->density_texture.width *
        volume_density->density_texture.height *
        volume_density->density_texture.depth;
    if (volume_density->density_texture.identifier == 0 || texel_count <= 0)
    {
        return false;
    }

    f32 *density_values = (f32 *) malloc((size_t) texel_count * sizeof(f32));
    if (density_values == NULL)
    {
        return false;
    }

    bool read_success = OpenGL_ReadTexture3D(
        &volume_density->density_texture,
        GL_RED,
        GL_FLOAT,
        density_values,
        (i32) (texel_count * sizeof(f32)));

    if (!read_success)
    {
        free(density_values);
        return false;
    }

    summary->minimum_density = FLT_MAX;
    summary->maximum_density = -FLT_MAX;
    summary->average_density = 0.0f;
    summary->center_slice_average_density = 0.0f;
    summary->center_voxel_density = 0.0f;

    i32 width = volume_density->density_texture.width;
    i32 height = volume_density->density_texture.height;
    i32 depth = volume_density->density_texture.depth;
    i32 center_z_index = depth / 2;
    i32 center_y_index = height / 2;
    i32 center_x_index = width / 2;

    for (i32 texel_index = 0; texel_index < texel_count; texel_index++)
    {
        f32 density_value = density_values[texel_index];
        if (density_value < summary->minimum_density) summary->minimum_density = density_value;
        if (density_value > summary->maximum_density) summary->maximum_density = density_value;
        summary->average_density += density_value;
    }

    i32 center_slice_texel_count = width * height;
    i32 center_slice_start_index = center_z_index * center_slice_texel_count;
    for (i32 center_slice_offset = 0; center_slice_offset < center_slice_texel_count; center_slice_offset++)
    {
        summary->center_slice_average_density += density_values[center_slice_start_index + center_slice_offset];
    }

    summary->average_density /= (f32) texel_count;
    summary->center_slice_average_density /= (f32) center_slice_texel_count;
    summary->center_voxel_density =
        density_values[center_slice_start_index + center_y_index * width + center_x_index];

    free(density_values);
    return true;
}

bool SimulationVolumeDensity_RunValidation (void)
{
    SimulationSpawnBox spawn_box = {0};
    spawn_box.center = Vec3_Create(0.0f, 0.0f, 0.0f);
    spawn_box.size = Vec3_Create(2.0f, 2.0f, 2.0f);
    spawn_box.particle_spacing = 0.5f;
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

    SimulationVolumeDensitySettings settings = {0};
    settings.bounds_size = Vec3_Create(4.0f, 4.0f, 4.0f);
    settings.smoothing_radius = 0.75f;
    settings.resolution_x = 16;
    settings.resolution_y = 16;
    settings.resolution_z = 16;
    settings.density_scale = 1.0f;

    SimulationVolumeDensity volume_density = {0};
    if (!SimulationVolumeDensity_Initialize(&volume_density, settings))
    {
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    bool run_success = SimulationVolumeDensity_Run(&volume_density, &particle_buffers, settings);
    SimulationVolumeDensityReadbackSummary summary = {0};
    bool readback_success = run_success && SimulationVolumeDensity_ReadbackSummary(&volume_density, &summary);

    bool validation_success =
        readback_success &&
        summary.maximum_density > 0.0f &&
        summary.center_voxel_density > summary.average_density &&
        summary.center_slice_average_density > 0.0f &&
        summary.maximum_density > summary.minimum_density + BASE_EPSILON32;

    SimulationVolumeDensity_Shutdown(&volume_density);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Volume density validation failed.");
        return false;
    }

    Base_LogInfo("Volume density validation passed.");
    return true;
}
