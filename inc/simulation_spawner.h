#ifndef SIMULATION_SPAWNER_H
#define SIMULATION_SPAWNER_H

#include "base.h"

typedef struct SimulationSpawnBox
{
    Vec3 center;
    Vec3 size;
    f32 particle_spacing;
    f32 position_jitter_scale;
    Vec3 initial_velocity;
} SimulationSpawnBox;

typedef struct SimulationSpawnData
{
    u32 particle_count;
    Vec4 *positions;
    Vec4 *predicted_positions;
    Vec4 *velocities;
    Vec4 *densities;
} SimulationSpawnData;

bool Simulation_GenerateSpawnDataBox (SimulationSpawnData *spawn_data, SimulationSpawnBox spawn_box);
void Simulation_ReleaseSpawnData (SimulationSpawnData *spawn_data);
bool Simulation_RunSpawnerValidation (void);

#endif // SIMULATION_SPAWNER_H
