#include <stdlib.h>
#include <string.h>

#include "simulation_data.h"

_Static_assert(sizeof(Vec4) == 16, "Vec4 must stay 16 bytes for GPU buffer compatibility.");
_Static_assert(sizeof(SimulationSettingsGpu) == 64, "SimulationSettingsGpu must stay 64 bytes.");

static void Simulation_FillInitialParticleData (
    Vec4 *position_data,
    Vec4 *predicted_position_data,
    Vec4 *velocity_data,
    Vec4 *density_data,
    u32 particle_count)
{
    for (u32 particle_index = 0; particle_index < particle_count; particle_index++)
    {
        f32 particle_index_f32 = (f32) particle_index;

        position_data[particle_index] = Vec4_Create(particle_index_f32, particle_index_f32 + 10.0f, 0.0f, 1.0f);
        predicted_position_data[particle_index] = position_data[particle_index];
        velocity_data[particle_index] = Vec4_Create(0.0f, 0.0f, 0.0f, 0.0f);
        density_data[particle_index] = Vec4_Create(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

bool Simulation_CreateParticleBuffersFromSpawnData (SimulationParticleBuffers *particle_buffers, const SimulationSpawnData *spawn_data)
{
    Base_Assert(particle_buffers != NULL);
    Base_Assert(spawn_data != NULL);

    if (spawn_data->particle_count == 0 ||
        spawn_data->positions == NULL ||
        spawn_data->predicted_positions == NULL ||
        spawn_data->velocities == NULL ||
        spawn_data->densities == NULL)
    {
        Base_LogError("Spawn data is incomplete.");
        return false;
    }

    memset(particle_buffers, 0, sizeof(*particle_buffers));

    bool success =
        OpenGL_CreateBuffer(
            &particle_buffers->position_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            spawn_data->positions,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->predicted_position_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            spawn_data->predicted_positions,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->velocity_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            spawn_data->velocities,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->density_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            spawn_data->densities,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->sort_target_position_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            NULL,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->sort_target_predicted_position_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            NULL,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->sort_target_velocity_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(Vec4)),
            NULL,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->spatial_key_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(u32)),
            NULL,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->spatial_hash_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(u32)),
            NULL,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->spatial_index_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(u32)),
            NULL,
            GL_DYNAMIC_COPY) &&
        OpenGL_CreateBuffer(
            &particle_buffers->spatial_offset_buffer,
            GL_SHADER_STORAGE_BUFFER,
            (i32) (spawn_data->particle_count * sizeof(u32)),
            NULL,
            GL_DYNAMIC_COPY);

    if (!success)
    {
        Simulation_DestroyParticleBuffers(particle_buffers);
        Base_LogError("Failed to create particle buffers from spawn data.");
        return false;
    }

    particle_buffers->particle_count = spawn_data->particle_count;
    return true;
}

bool Simulation_CreateParticleBuffers (SimulationParticleBuffers *particle_buffers, u32 particle_count)
{
    Base_Assert(particle_buffers != NULL);

    memset(particle_buffers, 0, sizeof(*particle_buffers));

    Vec4 *position_data = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *predicted_position_data = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *velocity_data = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *density_data = (Vec4 *) calloc(particle_count, sizeof(Vec4));

    if (position_data == NULL ||
        predicted_position_data == NULL ||
        velocity_data == NULL ||
        density_data == NULL)
    {
        free(position_data);
        free(predicted_position_data);
        free(velocity_data);
        free(density_data);
        Base_LogError("Failed to allocate CPU-side particle initialization data.");
        return false;
    }

    Simulation_FillInitialParticleData(
        position_data,
        predicted_position_data,
        velocity_data,
        density_data,
        particle_count);

    SimulationSpawnData spawn_data = {0};
    spawn_data.particle_count = particle_count;
    spawn_data.positions = position_data;
    spawn_data.predicted_positions = predicted_position_data;
    spawn_data.velocities = velocity_data;
    spawn_data.densities = density_data;

    bool success = Simulation_CreateParticleBuffersFromSpawnData(particle_buffers, &spawn_data);

    free(position_data);
    free(predicted_position_data);
    free(velocity_data);
    free(density_data);

    if (!success)
    {
        Simulation_DestroyParticleBuffers(particle_buffers);
        Base_LogError("Failed to create one or more simulation particle buffers.");
        return false;
    }

    particle_buffers->particle_count = particle_count;
    return true;
}

void Simulation_DestroyParticleBuffers (SimulationParticleBuffers *particle_buffers)
{
    if (particle_buffers == NULL) return;

    OpenGL_DestroyBuffer(&particle_buffers->position_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->predicted_position_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->velocity_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->density_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->sort_target_position_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->sort_target_predicted_position_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->sort_target_velocity_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->spatial_key_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->spatial_hash_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->spatial_index_buffer);
    OpenGL_DestroyBuffer(&particle_buffers->spatial_offset_buffer);
    particle_buffers->particle_count = 0;
}

bool Simulation_RunDataModelValidation (void)
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

    enum { validation_particle_count = 32 };
    Vec4 readback_positions[validation_particle_count];
    Vec4 readback_predicted_positions[validation_particle_count];

    bool position_read_success = OpenGL_ReadBuffer(
        &particle_buffers.position_buffer,
        readback_positions,
        (i32) sizeof(readback_positions));

    bool predicted_read_success = OpenGL_ReadBuffer(
        &particle_buffers.predicted_position_buffer,
        readback_predicted_positions,
        (i32) sizeof(readback_predicted_positions));

    if (!position_read_success || !predicted_read_success)
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        Simulation_DestroyParticleBuffers(&particle_buffers);
        return false;
    }

    Vec4 first_position = readback_positions[0];
    Vec4 last_position = readback_positions[validation_particle_count - 1];
    Vec4 last_predicted_position = readback_predicted_positions[validation_particle_count - 1];

    bool is_valid =
        Base_F32NearlyEqual(first_position.x, -1.5f, BASE_EPSILON32) &&
        Base_F32NearlyEqual(first_position.y, -1.5f, BASE_EPSILON32) &&
        Base_F32NearlyEqual(last_position.x, 1.5f, BASE_EPSILON32) &&
        Base_F32NearlyEqual(last_position.y, 1.5f, BASE_EPSILON32) &&
        Base_F32NearlyEqual(last_position.z, 0.5f, BASE_EPSILON32) &&
        Base_F32NearlyEqual(last_predicted_position.x, 1.5f, BASE_EPSILON32) &&
        Base_F32NearlyEqual(last_predicted_position.y, 1.5f, BASE_EPSILON32) &&
        particle_buffers.particle_count == validation_particle_count &&
        particle_buffers.position_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.predicted_position_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.velocity_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.density_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.sort_target_position_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.sort_target_predicted_position_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.sort_target_velocity_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(Vec4)) &&
        particle_buffers.spatial_key_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(u32)) &&
        particle_buffers.spatial_hash_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(u32)) &&
        particle_buffers.spatial_index_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(u32)) &&
        particle_buffers.spatial_offset_buffer.size_in_bytes == (i32) (validation_particle_count * sizeof(u32));

    Simulation_ReleaseSpawnData(&spawn_data);
    Simulation_DestroyParticleBuffers(&particle_buffers);

    if (!is_valid)
    {
        Base_LogError("Simulation data model validation failed.");
        return false;
    }

    Base_LogInfo("Simulation data model validation passed.");
    return true;
}
