#ifndef SIMULATION_VISCOSITY_H
#define SIMULATION_VISCOSITY_H

#include "simulation_data.h"

typedef struct SimulationViscositySettings
{
    f32 smoothing_radius;
    f32 viscosity_strength;
} SimulationViscositySettings;

typedef struct SimulationViscosity
{
    u32 viscosity_program_identifier;
    i32 smoothing_radius_uniform;
    i32 particle_count_uniform;
    i32 table_size_uniform;
    i32 delta_time_uniform;
    i32 viscosity_strength_uniform;
} SimulationViscosity;

bool SimulationViscosity_Initialize (SimulationViscosity *viscosity);
void SimulationViscosity_Shutdown (SimulationViscosity *viscosity);
bool SimulationViscosity_Run (
    SimulationViscosity *viscosity,
    SimulationParticleBuffers *particle_buffers,
    SimulationViscositySettings settings,
    f32 delta_time_seconds);
bool SimulationViscosity_RunValidation (void);

#endif // SIMULATION_VISCOSITY_H
