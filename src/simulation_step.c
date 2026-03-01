#include "opengl_helpers.h"
#include "simulation_step.h"

bool SimulationStepper_Initialize (SimulationStepper *stepper)
{
    Base_Assert(stepper != NULL);

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    stepper->external_forces_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/external_forces.comp");
    if (stepper->external_forces_program_identifier == 0)
    {
        Base_LogError("Failed to create the external forces compute program.");
        return false;
    }

    stepper->gravity_uniform = glGetUniformLocation(stepper->external_forces_program_identifier, "u_gravity");
    stepper->delta_time_uniform = glGetUniformLocation(stepper->external_forces_program_identifier, "u_delta_time");
    stepper->prediction_factor_uniform = glGetUniformLocation(stepper->external_forces_program_identifier, "u_prediction_factor");
    stepper->particle_count_uniform = glGetUniformLocation(stepper->external_forces_program_identifier, "u_particle_count");
    return true;
}

void SimulationStepper_Shutdown (SimulationStepper *stepper)
{
    if (stepper == NULL) return;

    if (stepper->external_forces_program_identifier != 0)
    {
        glDeleteProgram(stepper->external_forces_program_identifier);
    }

    stepper->external_forces_program_identifier = 0;
    stepper->gravity_uniform = -1;
    stepper->delta_time_uniform = -1;
    stepper->prediction_factor_uniform = -1;
    stepper->particle_count_uniform = -1;
}

bool SimulationStepper_RunSubstep (
    SimulationStepper *stepper,
    const SimulationParticleBuffers *particle_buffers,
    SimulationStepSettings settings,
    f32 delta_time_seconds)
{
    Base_Assert(stepper != NULL);
    Base_Assert(particle_buffers != NULL);

    if (stepper->external_forces_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return true;
    if (delta_time_seconds < 0.0f)
    {
        Base_LogError("Simulation substep delta time must be non-negative.");
        return false;
    }

    u32 workgroup_count_x = (particle_buffers->particle_count + 63u) / 64u;

    glUseProgram(stepper->external_forces_program_identifier);
    glUniform1f(stepper->gravity_uniform, settings.gravity);
    glUniform1f(stepper->prediction_factor_uniform, settings.prediction_factor);
    glUniform1i(stepper->particle_count_uniform, (i32) particle_buffers->particle_count);
    glUniform1f(stepper->delta_time_uniform, delta_time_seconds);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffers->position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->velocity_buffer.identifier);

    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glUseProgram(0);
    return true;
}

void SimulationStepper_RunFrame (
    SimulationStepper *stepper,
    const SimulationParticleBuffers *particle_buffers,
    SimulationStepSettings settings,
    f32 frame_delta_time_seconds)
{
    Base_Assert(stepper != NULL);
    Base_Assert(particle_buffers != NULL);

    i32 iterations_per_frame = settings.iterations_per_frame;
    if (iterations_per_frame < 1) iterations_per_frame = 1;

    f32 clamped_frame_delta_time = frame_delta_time_seconds * settings.time_scale;
    if (settings.maximum_delta_time > 0.0f)
    {
        clamped_frame_delta_time = Base_ClampF32(clamped_frame_delta_time, 0.0f, settings.maximum_delta_time);
    }

    f32 substep_delta_time = clamped_frame_delta_time / (f32) iterations_per_frame;

    for (i32 iteration_index = 0; iteration_index < iterations_per_frame; iteration_index++)
    {
        if (!SimulationStepper_RunSubstep(stepper, particle_buffers, settings, substep_delta_time))
        {
            return;
        }
    }
}
