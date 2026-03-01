#ifndef SIMULATION_STEP_H
#define SIMULATION_STEP_H

#include "base.h"
#include "simulation_data.h"

typedef struct SimulationStepSettings
{
    f32 gravity;
    f32 prediction_factor;
    f32 time_scale;
    f32 maximum_delta_time;
    i32 iterations_per_frame;
} SimulationStepSettings;

typedef struct SimulationStepper
{
    u32 external_forces_program_identifier;
    i32 gravity_uniform;
    i32 delta_time_uniform;
    i32 prediction_factor_uniform;
    i32 particle_count_uniform;
} SimulationStepper;

bool SimulationStepper_Initialize (SimulationStepper *stepper);
void SimulationStepper_Shutdown (SimulationStepper *stepper);
bool SimulationStepper_RunSubstep (
    SimulationStepper *stepper,
    const SimulationParticleBuffers *particle_buffers,
    SimulationStepSettings settings,
    f32 delta_time_seconds);
void SimulationStepper_RunFrame (
    SimulationStepper *stepper,
    const SimulationParticleBuffers *particle_buffers,
    SimulationStepSettings settings,
    f32 frame_delta_time_seconds);

#endif // SIMULATION_STEP_H
