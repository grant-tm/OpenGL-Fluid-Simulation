#ifndef SIMULATION_DENSITY_H
#define SIMULATION_DENSITY_H

#include "simulation_data.h"

typedef struct SimulationDensitySettings
{
    f32 smoothing_radius;
} SimulationDensitySettings;

typedef struct SimulationDensity
{
    u32 density_program_identifier;
    i32 smoothing_radius_uniform;
    i32 particle_count_uniform;
    i32 table_size_uniform;
} SimulationDensity;

bool SimulationDensity_Initialize (SimulationDensity *density);
void SimulationDensity_Shutdown (SimulationDensity *density);
bool SimulationDensity_Run (
    SimulationDensity *density,
    SimulationParticleBuffers *particle_buffers,
    SimulationDensitySettings settings);
bool SimulationDensity_RunValidation (void);

#endif // SIMULATION_DENSITY_H
