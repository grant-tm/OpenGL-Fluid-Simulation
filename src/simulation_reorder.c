#include <stdlib.h>
#include <string.h>

#include "simulation_reorder.h"

static bool SimulationReorder_ReadParticleData (
    SimulationParticleBuffers *particle_buffers,
    Vec4 *positions,
    Vec4 *predicted_positions,
    Vec4 *velocities,
    u32 *sorted_indices)
{
    u32 particle_count = particle_buffers->particle_count;

    return
        OpenGL_ReadBuffer(&particle_buffers->position_buffer, positions, (i32) (particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(&particle_buffers->predicted_position_buffer, predicted_positions, (i32) (particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(&particle_buffers->velocity_buffer, velocities, (i32) (particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(&particle_buffers->spatial_index_buffer, sorted_indices, (i32) (particle_count * sizeof(u32)));
}

static bool SimulationReorder_UploadParticleData (
    SimulationParticleBuffers *particle_buffers,
    Vec4 *positions,
    Vec4 *predicted_positions,
    Vec4 *velocities)
{
    u32 particle_count = particle_buffers->particle_count;

    return
        OpenGL_UpdateBuffer(&particle_buffers->sort_target_position_buffer, 0, (i32) (particle_count * sizeof(Vec4)), positions) &&
        OpenGL_UpdateBuffer(&particle_buffers->sort_target_predicted_position_buffer, 0, (i32) (particle_count * sizeof(Vec4)), predicted_positions) &&
        OpenGL_UpdateBuffer(&particle_buffers->sort_target_velocity_buffer, 0, (i32) (particle_count * sizeof(Vec4)), velocities) &&
        OpenGL_UpdateBuffer(&particle_buffers->position_buffer, 0, (i32) (particle_count * sizeof(Vec4)), positions) &&
        OpenGL_UpdateBuffer(&particle_buffers->predicted_position_buffer, 0, (i32) (particle_count * sizeof(Vec4)), predicted_positions) &&
        OpenGL_UpdateBuffer(&particle_buffers->velocity_buffer, 0, (i32) (particle_count * sizeof(Vec4)), velocities);
}

bool SimulationReorder_Run (SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(particle_buffers != NULL);

    u32 particle_count = particle_buffers->particle_count;
    if (particle_count == 0) return false;

    Vec4 *positions = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *predicted_positions = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *velocities = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    u32 *sorted_indices = (u32 *) calloc(particle_count, sizeof(u32));

    Vec4 *sorted_positions = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *sorted_predicted_positions = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    Vec4 *sorted_velocities = (Vec4 *) calloc(particle_count, sizeof(Vec4));

    if (positions == NULL ||
        predicted_positions == NULL ||
        velocities == NULL ||
        sorted_indices == NULL ||
        sorted_positions == NULL ||
        sorted_predicted_positions == NULL ||
        sorted_velocities == NULL)
    {
        free(positions);
        free(predicted_positions);
        free(velocities);
        free(sorted_indices);
        free(sorted_positions);
        free(sorted_predicted_positions);
        free(sorted_velocities);
        Base_LogError("Failed to allocate particle reorder buffers.");
        return false;
    }

    bool read_success = SimulationReorder_ReadParticleData(
        particle_buffers,
        positions,
        predicted_positions,
        velocities,
        sorted_indices);

    if (!read_success)
    {
        free(positions);
        free(predicted_positions);
        free(velocities);
        free(sorted_indices);
        free(sorted_positions);
        free(sorted_predicted_positions);
        free(sorted_velocities);
        return false;
    }

    for (u32 sorted_index = 0; sorted_index < particle_count; sorted_index++)
    {
        u32 source_index = sorted_indices[sorted_index];
        if (source_index >= particle_count)
        {
            free(positions);
            free(predicted_positions);
            free(velocities);
            free(sorted_indices);
            free(sorted_positions);
            free(sorted_predicted_positions);
            free(sorted_velocities);
            Base_LogError("Particle reorder encountered an out-of-range source index.");
            return false;
        }

        sorted_positions[sorted_index] = positions[source_index];
        sorted_predicted_positions[sorted_index] = predicted_positions[source_index];
        sorted_velocities[sorted_index] = velocities[source_index];
    }

    bool upload_success = SimulationReorder_UploadParticleData(
        particle_buffers,
        sorted_positions,
        sorted_predicted_positions,
        sorted_velocities);

    free(positions);
    free(predicted_positions);
    free(velocities);
    free(sorted_indices);
    free(sorted_positions);
    free(sorted_predicted_positions);
    free(sorted_velocities);
    return upload_success;
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
