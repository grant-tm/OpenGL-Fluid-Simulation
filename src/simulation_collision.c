#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "opengl_helpers.h"
#include "simulation_collision.h"

bool SimulationCollision_Initialize (SimulationCollision *collision)
{
    Base_Assert(collision != NULL);
    memset(collision, 0, sizeof(*collision));

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    collision->collision_program_identifier = OpenGL_CreateComputeProgramFromFile("assets/shaders/collision.comp");
    if (collision->collision_program_identifier == 0)
    {
        Base_LogError("Failed to create the collision compute program.");
        return false;
    }

    collision->particle_count_uniform = glGetUniformLocation(collision->collision_program_identifier, "u_particle_count");
    collision->bounds_size_uniform = glGetUniformLocation(collision->collision_program_identifier, "u_bounds_size");
    collision->collision_damping_uniform = glGetUniformLocation(collision->collision_program_identifier, "u_collision_damping");
    collision->minimum_bounce_speed_uniform = glGetUniformLocation(collision->collision_program_identifier, "u_minimum_bounce_speed");
    return true;
}

void SimulationCollision_Shutdown (SimulationCollision *collision)
{
    if (collision == NULL) return;

    if (collision->collision_program_identifier != 0)
    {
        glDeleteProgram(collision->collision_program_identifier);
    }

    memset(collision, 0, sizeof(*collision));
}

bool SimulationCollision_Run (
    SimulationCollision *collision,
    SimulationParticleBuffers *particle_buffers,
    SimulationCollisionSettings settings)
{
    Base_Assert(collision != NULL);
    Base_Assert(particle_buffers != NULL);

    if (collision->collision_program_identifier == 0) return false;
    if (particle_buffers->particle_count == 0) return false;

    u32 workgroup_count_x = (particle_buffers->particle_count + 63u) / 64u;

    glUseProgram(collision->collision_program_identifier);
    glUniform1i(collision->particle_count_uniform, (i32) particle_buffers->particle_count);
    glUniform3f(collision->bounds_size_uniform, settings.bounds_size.x, settings.bounds_size.y, settings.bounds_size.z);
    glUniform1f(collision->collision_damping_uniform, settings.collision_damping);
    glUniform1f(collision->minimum_bounce_speed_uniform, settings.minimum_bounce_speed);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particle_buffers->predicted_position_buffer.identifier);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particle_buffers->velocity_buffer.identifier);

    OpenGL_DispatchCompute(workgroup_count_x, 1, 1);
    OpenGL_MemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_BUFFER_UPDATE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glUseProgram(0);
    return true;
}

bool SimulationCollision_RunValidation (void)
{
    SimulationSpawnData spawn_data = {0};
    spawn_data.particle_count = 1;
    spawn_data.positions = (Vec4 *) calloc(1, sizeof(Vec4));
    spawn_data.predicted_positions = (Vec4 *) calloc(1, sizeof(Vec4));
    spawn_data.velocities = (Vec4 *) calloc(1, sizeof(Vec4));
    spawn_data.densities = (Vec4 *) calloc(1, sizeof(Vec4));

    if (spawn_data.positions == NULL ||
        spawn_data.predicted_positions == NULL ||
        spawn_data.velocities == NULL ||
        spawn_data.densities == NULL)
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        Base_LogError("Failed to allocate collision validation data.");
        return false;
    }

    spawn_data.positions[0] = Vec4_Create(0.0f, 0.0f, 0.0f, 1.0f);
    spawn_data.predicted_positions[0] = Vec4_Create(0.0f, 5.0f, 0.0f, 1.0f);
    spawn_data.velocities[0] = Vec4_Create(0.0f, 3.0f, 0.0f, 0.0f);
    spawn_data.densities[0] = Vec4_Create(0.0f, 0.0f, 0.0f, 0.0f);

    SimulationParticleBuffers particle_buffers = {0};
    if (!Simulation_CreateParticleBuffersFromSpawnData(&particle_buffers, &spawn_data))
    {
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationCollision collision = {0};
    if (!SimulationCollision_Initialize(&collision))
    {
        Simulation_DestroyParticleBuffers(&particle_buffers);
        Simulation_ReleaseSpawnData(&spawn_data);
        return false;
    }

    SimulationCollisionSettings settings = {0};
    settings.bounds_size = Vec3_Create(8.0f, 8.0f, 8.0f);
    settings.collision_damping = 0.5f;
    settings.minimum_bounce_speed = 0.0f;

    bool run_success = SimulationCollision_Run(&collision, &particle_buffers, settings);

    Vec4 readback_predicted_position = {0};
    Vec4 readback_velocity = {0};
    bool read_success = run_success &&
        OpenGL_ReadBuffer(&particle_buffers.predicted_position_buffer, &readback_predicted_position, (i32) sizeof(readback_predicted_position)) &&
        OpenGL_ReadBuffer(&particle_buffers.velocity_buffer, &readback_velocity, (i32) sizeof(readback_velocity));

    bool validation_success = read_success &&
        fabsf(readback_predicted_position.y - 4.0f) <= BASE_EPSILON32 &&
        fabsf(readback_velocity.y + 1.5f) <= BASE_EPSILON32;

    SimulationCollision_Shutdown(&collision);
    Simulation_DestroyParticleBuffers(&particle_buffers);
    Simulation_ReleaseSpawnData(&spawn_data);

    if (!validation_success)
    {
        Base_LogError("Collision validation failed.");
        return false;
    }

    Base_LogInfo("Collision validation passed.");
    return true;
}
