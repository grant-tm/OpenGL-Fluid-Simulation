#ifndef SIMULATION_PIPELINE_H
#define SIMULATION_PIPELINE_H

#include "simulation_collision.h"
#include "simulation_density.h"
#include "simulation_pressure.h"
#include "simulation_spatial_hash.h"
#include "simulation_step.h"
#include "simulation_viscosity.h"
#include "simulation_whitewater.h"

typedef struct SimulationPipeline
{
    struct SimulationPipelineDebugTimings
    {
        f64 external_forces_milliseconds;
        f64 spatial_hash_milliseconds;
        f64 reorder_milliseconds;
        f64 density_milliseconds;
        f64 pressure_milliseconds;
        f64 viscosity_milliseconds;
        f64 collision_milliseconds;
        f64 integrate_positions_milliseconds;
        f64 total_milliseconds;
    } last_debug_timings;
    u32 integrate_positions_program_identifier;
    i32 particle_count_uniform;
} SimulationPipeline;

typedef struct SimulationPipelineSettings
{
    i32 substeps_per_simulation_step;
    f32 time_scale;
    SimulationStepSettings step_settings;
    SimulationSpatialHashSettings spatial_hash_settings;
    SimulationDensitySettings density_settings;
    SimulationPressureSettings pressure_settings;
    SimulationViscositySettings viscosity_settings;
    SimulationCollisionSettings collision_settings;
} SimulationPipelineSettings;

bool SimulationPipeline_Initialize (SimulationPipeline *pipeline);
void SimulationPipeline_Shutdown (SimulationPipeline *pipeline);
bool SimulationPipeline_RebuildDerivedState (
    SimulationParticleBuffers *particle_buffers,
    SimulationSpatialHash *spatial_hash,
    SimulationDensity *density,
    SimulationPipelineSettings settings);
bool SimulationPipeline_RunSimulationStep (
    SimulationPipeline *pipeline,
    SimulationStepper *stepper,
    SimulationSpatialHash *spatial_hash,
    SimulationDensity *density,
    SimulationPressure *pressure,
    SimulationViscosity *viscosity,
    SimulationCollision *collision,
    SimulationWhitewater *whitewater,
    SimulationParticleBuffers *particle_buffers,
    SimulationPipelineSettings settings,
    const SimulationWhitewaterSettings *whitewater_settings,
    f32 simulation_delta_time_seconds);
bool SimulationPipeline_RunValidation (void);

#endif // SIMULATION_PIPELINE_H
