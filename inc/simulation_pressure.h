#ifndef SIMULATION_PRESSURE_H
#define SIMULATION_PRESSURE_H

#include "simulation_data.h"

typedef struct SimulationPressureSettings
{
    f32 smoothing_radius;
    f32 target_density;
    f32 pressure_multiplier;
    f32 near_pressure_multiplier;
} SimulationPressureSettings;

typedef struct SimulationPressure
{
    u32 pressure_program_identifier;
    i32 smoothing_radius_uniform;
    i32 particle_count_uniform;
    i32 table_size_uniform;
    i32 delta_time_uniform;
    i32 target_density_uniform;
    i32 pressure_multiplier_uniform;
    i32 near_pressure_multiplier_uniform;
} SimulationPressure;

bool SimulationPressure_Initialize (SimulationPressure *pressure);
void SimulationPressure_Shutdown (SimulationPressure *pressure);
bool SimulationPressure_Run (
    SimulationPressure *pressure,
    SimulationParticleBuffers *particle_buffers,
    SimulationPressureSettings settings,
    f32 delta_time_seconds);
bool SimulationPressure_RunValidation (void);

#endif // SIMULATION_PRESSURE_H
