#ifndef SIMULATION_VOLUME_DENSITY_H
#define SIMULATION_VOLUME_DENSITY_H

#include "simulation_data.h"

typedef struct SimulationVolumeDensitySettings
{
    Vec3 bounds_size;
    f32 smoothing_radius;
    i32 resolution_x;
    i32 resolution_y;
    i32 resolution_z;
    f32 density_scale;
} SimulationVolumeDensitySettings;

typedef struct SimulationVolumeDensityReadbackSummary
{
    f32 minimum_density;
    f32 maximum_density;
    f32 average_density;
    f32 center_slice_average_density;
    f32 center_voxel_density;
} SimulationVolumeDensityReadbackSummary;

typedef struct SimulationVolumeDensity
{
    OpenGLTexture3D density_texture;
    u32 density_volume_program_identifier;
    i32 particle_count_uniform;
    i32 bounds_size_uniform;
    i32 resolution_uniform;
    i32 smoothing_radius_uniform;
    i32 density_scale_uniform;
} SimulationVolumeDensity;

bool SimulationVolumeDensity_Initialize (
    SimulationVolumeDensity *volume_density,
    SimulationVolumeDensitySettings settings);
void SimulationVolumeDensity_Shutdown (SimulationVolumeDensity *volume_density);
bool SimulationVolumeDensity_Run (
    SimulationVolumeDensity *volume_density,
    const SimulationParticleBuffers *particle_buffers,
    SimulationVolumeDensitySettings settings);
bool SimulationVolumeDensity_ReadbackSummary (
    SimulationVolumeDensity *volume_density,
    SimulationVolumeDensityReadbackSummary *summary);
bool SimulationVolumeDensity_RunValidation (void);

#endif // SIMULATION_VOLUME_DENSITY_H
