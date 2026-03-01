#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "opengl_helpers.h"
#include "simulation_pipeline.h"
#include "simulation_reorder.h"
#include "simulation_spawner.h"

static f64 SimulationPipeline_GetMillisecondsBetweenCounters (
    LARGE_INTEGER start_counter,
    LARGE_INTEGER end_counter,
    LARGE_INTEGER frequency)
{
    return
        ((f64) (end_counter.QuadPart - start_counter.QuadPart) * 1000.0) /
        (f64) frequency.QuadPart;
}

static bool SimulationPipeline_CommitPredictedPositions (
    SimulationPipeline *pipeline,
    SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(pipeline != NULL);
    Base_Assert(particle_buffers != NULL);

    if (pipeline->integrate_positions_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return true;

    u32 workgroup_count_x = (particle_buffers->particle_count + 63u) / 64u;

    glUseProgram(pipeline->integrate_positions_program_identifier);
    glUniform1i(pipeline->particle_count_uniform, (i32) particle_buffers->particle_count);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);

    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glUseProgram(0);
    return true;
}

bool SimulationPipeline_Initialize (SimulationPipeline *pipeline)
{
    Base_Assert(pipeline != NULL);
    memset(pipeline, 0, sizeof(*pipeline));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    pipeline->integrate_positions_program_identifier =
        OpenGL_CreateComputeProgramFromFile("assets/shaders/integrate_positions.comp");
    if (pipeline->integrate_positions_program_identifier == 0)
    {
        Base_LogError("Failed to create the integrate positions compute program.");
        return false;
    }

    pipeline->particle_count_uniform =
        glGetUniformLocation(pipeline->integrate_positions_program_identifier, "u_particle_count");
    return true;
}

void SimulationPipeline_Shutdown (SimulationPipeline *pipeline)
{
    if (pipeline == NULL) return;

    if (pipeline->integrate_positions_program_identifier != 0)
    {
        glDeleteProgram(pipeline->integrate_positions_program_identifier);
    }

    memset(pipeline, 0, sizeof(*pipeline));
}

bool SimulationPipeline_RebuildDerivedState (
    SimulationParticleBuffers *particle_buffers,
    SimulationSpatialHash *spatial_hash,
    SimulationDensity *density,
    SimulationPipelineSettings settings)
{
    Base_Assert(particle_buffers != NULL);
    Base_Assert(spatial_hash != NULL);
    Base_Assert(density != NULL);

    return
        SimulationSpatialHash_Run(spatial_hash, particle_buffers, settings.spatial_hash_settings) &&
        SimulationReorder_Run(particle_buffers) &&
        SimulationDensity_Run(density, particle_buffers, settings.density_settings);
}

bool SimulationPipeline_RunSimulationStep (
    SimulationPipeline *pipeline,
    SimulationStepper *stepper,
    SimulationSpatialHash *spatial_hash,
    SimulationDensity *density,
    SimulationPressure *pressure,
    SimulationViscosity *viscosity,
    SimulationCollision *collision,
    SimulationParticleBuffers *particle_buffers,
    SimulationPipelineSettings settings,
    f32 simulation_delta_time_seconds)
{
    Base_Assert(pipeline != NULL);
    Base_Assert(stepper != NULL);
    Base_Assert(spatial_hash != NULL);
    Base_Assert(density != NULL);
    Base_Assert(pressure != NULL);
    Base_Assert(viscosity != NULL);
    Base_Assert(collision != NULL);
    Base_Assert(particle_buffers != NULL);

    i32 substeps_per_simulation_step = settings.substeps_per_simulation_step;
    if (substeps_per_simulation_step < 1)
    {
        substeps_per_simulation_step = 1;
    }

    f32 scaled_simulation_delta_time = simulation_delta_time_seconds * settings.time_scale;
    if (scaled_simulation_delta_time < 0.0f)
    {
        Base_LogError("Simulation delta time must be non-negative.");
        return false;
    }

    f32 substep_delta_time = scaled_simulation_delta_time / (f32) substeps_per_simulation_step;
    LARGE_INTEGER performance_frequency = {0};
    QueryPerformanceFrequency(&performance_frequency);
    memset(&pipeline->last_debug_timings, 0, sizeof(pipeline->last_debug_timings));

    LARGE_INTEGER total_start_counter = {0};
    LARGE_INTEGER total_end_counter = {0};
    QueryPerformanceCounter(&total_start_counter);

    for (i32 substep_index = 0; substep_index < substeps_per_simulation_step; substep_index++)
    {
        LARGE_INTEGER pass_start_counter = {0};
        LARGE_INTEGER pass_end_counter = {0};

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationStepper_RunSubstep(stepper, particle_buffers, settings.step_settings, substep_delta_time))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.external_forces_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationSpatialHash_Run(spatial_hash, particle_buffers, settings.spatial_hash_settings))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.spatial_hash_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationReorder_Run(particle_buffers))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.reorder_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationDensity_Run(density, particle_buffers, settings.density_settings))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.density_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationPressure_Run(pressure, particle_buffers, settings.pressure_settings, substep_delta_time))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.pressure_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationViscosity_Run(viscosity, particle_buffers, settings.viscosity_settings, substep_delta_time))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.viscosity_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationCollision_Run(collision, particle_buffers, settings.collision_settings))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.collision_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        if (!SimulationPipeline_CommitPredictedPositions(pipeline, particle_buffers))
        {
            return false;
        }
        QueryPerformanceCounter(&pass_end_counter);
        pipeline->last_debug_timings.integrate_positions_milliseconds +=
            SimulationPipeline_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);
    }

    QueryPerformanceCounter(&total_end_counter);
    pipeline->last_debug_timings.total_milliseconds =
        SimulationPipeline_GetMillisecondsBetweenCounters(total_start_counter, total_end_counter, performance_frequency);

    return true;
}

bool SimulationPipeline_RunValidation (void)
{
    SimulationSpawnBox spawn_box = {0};
    spawn_box.center = Vec3_Create(0.0f, 1.0f, 0.0f);
    spawn_box.size = Vec3_Create(2.0f, 2.0f, 2.0f);
    spawn_box.particle_spacing = 0.5f;
    spawn_box.initial_velocity = Vec3_Create(0.0f, 0.0f, 0.0f);

    SimulationSpawnData spawn_data = {0};
    if (!Simulation_GenerateSpawnDataBox(&spawn_data, spawn_box))
    {
        return false;
    }

    f32 initial_average_height = 0.0f;
    for (u32 particle_index = 0; particle_index < spawn_data.particle_count; particle_index++)
    {
        initial_average_height += spawn_data.positions[particle_index].y;
    }
    initial_average_height /= (f32) spawn_data.particle_count;

    SimulationParticleBuffers particle_buffers = {0};
    if (!Simulation_CreateParticleBuffersFromSpawnData(&particle_buffers, &spawn_data))
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationStepper stepper = {0};
    SimulationSpatialHash spatial_hash = {0};
    SimulationDensity density = {0};
    SimulationPressure pressure = {0};
    SimulationViscosity viscosity = {0};
    SimulationCollision collision = {0};
    SimulationPipeline pipeline = {0};

    bool initialize_success =
        SimulationStepper_Initialize(&stepper) &&
        SimulationSpatialHash_Initialize(&spatial_hash) &&
        SimulationDensity_Initialize(&density) &&
        SimulationPressure_Initialize(&pressure) &&
        SimulationViscosity_Initialize(&viscosity) &&
        SimulationCollision_Initialize(&collision) &&
        SimulationPipeline_Initialize(&pipeline);

    if (!initialize_success)
    {
        SimulationPipeline_Shutdown(&pipeline);
        SimulationCollision_Shutdown(&collision);
        SimulationViscosity_Shutdown(&viscosity);
        SimulationPressure_Shutdown(&pressure);
        SimulationDensity_Shutdown(&density);
        SimulationSpatialHash_Shutdown(&spatial_hash);
        SimulationStepper_Shutdown(&stepper);
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationPipelineSettings settings = {0};
    settings.substeps_per_simulation_step = 3;
    settings.time_scale = 1.0f;
    settings.step_settings.gravity = -9.8f;
    settings.step_settings.prediction_factor = 1.0f / 120.0f;
    settings.spatial_hash_settings.cell_size = 0.55f;
    settings.density_settings.smoothing_radius = 0.55f;
    settings.pressure_settings.smoothing_radius = 0.55f;
    settings.pressure_settings.target_density = 6.0f;
    settings.pressure_settings.pressure_multiplier = 18.0f;
    settings.pressure_settings.near_pressure_multiplier = 8.0f;
    settings.viscosity_settings.smoothing_radius = 0.55f;
    settings.viscosity_settings.viscosity_strength = 0.10f;
    settings.collision_settings.bounds_size = Vec3_Create(4.0f, 4.0f, 4.0f);
    settings.collision_settings.collision_damping = 0.85f;

    bool run_success = SimulationPipeline_RebuildDerivedState(&particle_buffers, &spatial_hash, &density, settings);

    for (i32 simulation_step_index = 0; run_success && simulation_step_index < 90; simulation_step_index++)
    {
        run_success = SimulationPipeline_RunSimulationStep(
            &pipeline,
            &stepper,
            &spatial_hash,
            &density,
            &pressure,
            &viscosity,
            &collision,
            &particle_buffers,
            settings,
            1.0f / 60.0f);
    }

    Vec4 *position_values = (Vec4 *) malloc((size_t) particle_buffers.particle_count * sizeof(Vec4));
    Vec4 *velocity_values = (Vec4 *) malloc((size_t) particle_buffers.particle_count * sizeof(Vec4));
    bool read_success =
        run_success &&
        position_values != NULL &&
        velocity_values != NULL &&
        OpenGL_ReadBuffer(
            &particle_buffers.position_buffer,
            position_values,
            (i32) (particle_buffers.particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(
            &particle_buffers.velocity_buffer,
            velocity_values,
            (i32) (particle_buffers.particle_count * sizeof(Vec4)));

    bool validation_success = read_success;
    f32 final_average_height = 0.0f;
    Vec3 half_bounds_size = Vec3_Scale(settings.collision_settings.bounds_size, 0.5f);

    if (validation_success)
    {
        for (u32 particle_index = 0; particle_index < particle_buffers.particle_count; particle_index++)
        {
            Vec4 position = position_values[particle_index];
            Vec4 velocity = velocity_values[particle_index];

            if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
                !isfinite(velocity.x) || !isfinite(velocity.y) || !isfinite(velocity.z))
            {
                validation_success = false;
                break;
            }

            if (position.x < -half_bounds_size.x - 0.01f || position.x > half_bounds_size.x + 0.01f ||
                position.y < -half_bounds_size.y - 0.01f || position.y > half_bounds_size.y + 0.01f ||
                position.z < -half_bounds_size.z - 0.01f || position.z > half_bounds_size.z + 0.01f)
            {
                validation_success = false;
                break;
            }

            final_average_height += position.y;
        }
    }

    if (validation_success)
    {
        final_average_height /= (f32) particle_buffers.particle_count;
        validation_success = final_average_height < initial_average_height - 0.05f;
    }

    free(position_values);
    free(velocity_values);

    SimulationPipeline_Shutdown(&pipeline);
    SimulationCollision_Shutdown(&collision);
    SimulationViscosity_Shutdown(&viscosity);
    SimulationPressure_Shutdown(&pressure);
    SimulationDensity_Shutdown(&density);
    SimulationSpatialHash_Shutdown(&spatial_hash);
    SimulationStepper_Shutdown(&stepper);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Stable simulation loop validation failed.");
        return false;
    }

    Base_LogInfo("Stable simulation loop validation passed.");
    return true;
}
