#include <stdlib.h>
#include <string.h>

#include "simulation_spawner.h"

static u32 Simulation_CalculateParticlesPerAxis (f32 axis_size, f32 particle_spacing)
{
    if (particle_spacing <= 0.0f) return 0;
    if (axis_size <= 0.0f) return 1;

    u32 particle_count = (u32) (axis_size / particle_spacing);
    if (particle_count == 0) particle_count = 1;
    return particle_count;
}

static u32 Simulation_HashU32 (u32 value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static f32 Simulation_RandomSignedNormalizedFromSeed (u32 seed)
{
    u32 hashed_value = Simulation_HashU32(seed);
    f32 normalized_value = (f32) hashed_value / (f32) 0xffffffffu;
    return normalized_value * 2.0f - 1.0f;
}

static bool Simulation_IsPositionWithinBounds (Vec4 position, SimulationSpawnBox spawn_box)
{
    f32 half_width = spawn_box.size.x * 0.5f;
    f32 half_height = spawn_box.size.y * 0.5f;
    f32 half_depth = spawn_box.size.z * 0.5f;

    bool is_within_x = position.x >= spawn_box.center.x - half_width - BASE_EPSILON32 &&
                       position.x <= spawn_box.center.x + half_width + BASE_EPSILON32;
    bool is_within_y = position.y >= spawn_box.center.y - half_height - BASE_EPSILON32 &&
                       position.y <= spawn_box.center.y + half_height + BASE_EPSILON32;
    bool is_within_z = position.z >= spawn_box.center.z - half_depth - BASE_EPSILON32 &&
                       position.z <= spawn_box.center.z + half_depth + BASE_EPSILON32;

    return is_within_x && is_within_y && is_within_z;
}

bool Simulation_GenerateSpawnDataBox (SimulationSpawnData *spawn_data, SimulationSpawnBox spawn_box)
{
    Base_Assert(spawn_data != NULL);
    memset(spawn_data, 0, sizeof(*spawn_data));

    if (spawn_box.particle_spacing <= 0.0f)
    {
        Base_LogError("Spawn box particle spacing must be greater than zero.");
        return false;
    }

    u32 particles_along_x = Simulation_CalculateParticlesPerAxis(spawn_box.size.x, spawn_box.particle_spacing);
    u32 particles_along_y = Simulation_CalculateParticlesPerAxis(spawn_box.size.y, spawn_box.particle_spacing);
    u32 particles_along_z = Simulation_CalculateParticlesPerAxis(spawn_box.size.z, spawn_box.particle_spacing);

    u32 particle_count = particles_along_x * particles_along_y * particles_along_z;
    if (particle_count == 0)
    {
        Base_LogError("Spawn box generated zero particles.");
        return false;
    }

    spawn_data->positions = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    spawn_data->predicted_positions = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    spawn_data->velocities = (Vec4 *) calloc(particle_count, sizeof(Vec4));
    spawn_data->densities = (Vec4 *) calloc(particle_count, sizeof(Vec4));

    if (spawn_data->positions == NULL ||
        spawn_data->predicted_positions == NULL ||
        spawn_data->velocities == NULL ||
        spawn_data->densities == NULL)
    {
        Simulation_ReleaseSpawnData(spawn_data);
        Base_LogError("Failed to allocate spawn data arrays.");
        return false;
    }

    f32 start_x = spawn_box.center.x - ((f32) (particles_along_x - 1) * spawn_box.particle_spacing) * 0.5f;
    f32 start_y = spawn_box.center.y - ((f32) (particles_along_y - 1) * spawn_box.particle_spacing) * 0.5f;
    f32 start_z = spawn_box.center.z - ((f32) (particles_along_z - 1) * spawn_box.particle_spacing) * 0.5f;

    u32 particle_index = 0;
    f32 jitter_distance = spawn_box.particle_spacing * spawn_box.position_jitter_scale;
    f32 minimum_x = spawn_box.center.x - spawn_box.size.x * 0.5f;
    f32 maximum_x = spawn_box.center.x + spawn_box.size.x * 0.5f;
    f32 minimum_y = spawn_box.center.y - spawn_box.size.y * 0.5f;
    f32 maximum_y = spawn_box.center.y + spawn_box.size.y * 0.5f;
    f32 minimum_z = spawn_box.center.z - spawn_box.size.z * 0.5f;
    f32 maximum_z = spawn_box.center.z + spawn_box.size.z * 0.5f;
    for (u32 z_index = 0; z_index < particles_along_z; z_index++)
    {
        for (u32 y_index = 0; y_index < particles_along_y; y_index++)
        {
            for (u32 x_index = 0; x_index < particles_along_x; x_index++)
            {
                f32 x_position = start_x + (f32) x_index * spawn_box.particle_spacing;
                f32 y_position = start_y + (f32) y_index * spawn_box.particle_spacing;
                f32 z_position = start_z + (f32) z_index * spawn_box.particle_spacing;

                if (jitter_distance > 0.0f)
                {
                    u32 base_seed = particle_index * 3u + 17u;
                    x_position += Simulation_RandomSignedNormalizedFromSeed(base_seed + 0u) * jitter_distance;
                    y_position += Simulation_RandomSignedNormalizedFromSeed(base_seed + 1u) * jitter_distance;
                    z_position += Simulation_RandomSignedNormalizedFromSeed(base_seed + 2u) * jitter_distance;
                    x_position = Base_ClampF32(x_position, minimum_x, maximum_x);
                    y_position = Base_ClampF32(y_position, minimum_y, maximum_y);
                    z_position = Base_ClampF32(z_position, minimum_z, maximum_z);
                }

                Vec4 position = Vec4_Create(x_position, y_position, z_position, 1.0f);
                Vec4 velocity = Vec4_Create(
                    spawn_box.initial_velocity.x,
                    spawn_box.initial_velocity.y,
                    spawn_box.initial_velocity.z,
                    0.0f);

                spawn_data->positions[particle_index] = position;
                spawn_data->predicted_positions[particle_index] = position;
                spawn_data->velocities[particle_index] = velocity;
                spawn_data->densities[particle_index] = Vec4_Create(0.0f, 0.0f, 0.0f, 0.0f);
                particle_index += 1;
            }
        }
    }

    spawn_data->particle_count = particle_count;
    return true;
}

void Simulation_ReleaseSpawnData (SimulationSpawnData *spawn_data)
{
    if (spawn_data == NULL) return;

    free(spawn_data->positions);
    free(spawn_data->predicted_positions);
    free(spawn_data->velocities);
    free(spawn_data->densities);

    memset(spawn_data, 0, sizeof(*spawn_data));
}

bool Simulation_RunSpawnerValidation (void)
{
    SimulationSpawnBox spawn_box = {0};
    spawn_box.center = Vec3_Create(0.0f, 1.0f, -2.0f);
    spawn_box.size = Vec3_Create(4.0f, 2.0f, 3.0f);
    spawn_box.particle_spacing = 1.0f;
    spawn_box.initial_velocity = Vec3_Create(0.5f, -1.0f, 2.0f);

    SimulationSpawnData spawn_data = {0};
    if (!Simulation_GenerateSpawnDataBox(&spawn_data, spawn_box))
    {
        return false;
    }

    bool is_valid = true;
    is_valid = is_valid && spawn_data.particle_count == 24;
    is_valid = is_valid && spawn_data.positions != NULL;
    is_valid = is_valid && spawn_data.predicted_positions != NULL;
    is_valid = is_valid && spawn_data.velocities != NULL;
    is_valid = is_valid && spawn_data.densities != NULL;

    if (is_valid)
    {
        Vec4 first_position = spawn_data.positions[0];
        Vec4 last_position = spawn_data.positions[spawn_data.particle_count - 1];
        Vec4 first_velocity = spawn_data.velocities[0];

        is_valid = is_valid && Base_F32NearlyEqual(first_position.x, -1.5f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(first_position.y, 0.5f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(first_position.z, -3.0f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(last_position.x, 1.5f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(last_position.y, 1.5f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(last_position.z, -1.0f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(first_velocity.x, 0.5f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(first_velocity.y, -1.0f, BASE_EPSILON32);
        is_valid = is_valid && Base_F32NearlyEqual(first_velocity.z, 2.0f, BASE_EPSILON32);
    }

    if (is_valid)
    {
        for (u32 particle_index = 0; particle_index < spawn_data.particle_count; particle_index++)
        {
            if (!Simulation_IsPositionWithinBounds(spawn_data.positions[particle_index], spawn_box))
            {
                is_valid = false;
                break;
            }

            Vec4 position = spawn_data.positions[particle_index];
            Vec4 predicted_position = spawn_data.predicted_positions[particle_index];
            is_valid = is_valid &&
                Base_F32NearlyEqual(position.x, predicted_position.x, BASE_EPSILON32) &&
                Base_F32NearlyEqual(position.y, predicted_position.y, BASE_EPSILON32) &&
                Base_F32NearlyEqual(position.z, predicted_position.z, BASE_EPSILON32);

            if (!is_valid) break;
        }
    }

    Simulation_ReleaseSpawnData(&spawn_data);

    if (!is_valid)
    {
        Base_LogError("Simulation spawner validation failed.");
        return false;
    }

    Base_LogInfo("Simulation spawner validation passed.");
    return true;
}
