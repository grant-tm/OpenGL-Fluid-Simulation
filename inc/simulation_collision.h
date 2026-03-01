#ifndef SIMULATION_COLLISION_H
#define SIMULATION_COLLISION_H

#include "simulation_data.h"

typedef struct SimulationCollisionSettings
{
    Vec3 bounds_size;
    f32 collision_damping;
} SimulationCollisionSettings;

typedef struct SimulationCollision
{
    u32 collision_program_identifier;
    i32 particle_count_uniform;
    i32 bounds_size_uniform;
    i32 collision_damping_uniform;
} SimulationCollision;

bool SimulationCollision_Initialize (SimulationCollision *collision);
void SimulationCollision_Shutdown (SimulationCollision *collision);
bool SimulationCollision_Run (
    SimulationCollision *collision,
    SimulationParticleBuffers *particle_buffers,
    SimulationCollisionSettings settings);
bool SimulationCollision_RunValidation (void);

#endif // SIMULATION_COLLISION_H
