struct SimulationSettingsGpu
{
    vec4 gravity_and_delta_time;
    vec4 bounds_size_and_collision_damping;
    vec4 density_parameters;
    vec4 viscosity_and_miscellaneous;
};

layout(std430, binding = 0) buffer ParticlePositions
{
    vec4 positions[];
};

layout(std430, binding = 1) buffer ParticlePredictedPositions
{
    vec4 predicted_positions[];
};

layout(std430, binding = 2) buffer ParticleVelocities
{
    vec4 velocities[];
};

layout(std430, binding = 3) buffer ParticleDensities
{
    vec4 densities[];
};

layout(std430, binding = 4) buffer SpatialKeys
{
    uint spatial_keys[];
};

layout(std430, binding = 5) buffer SpatialHashes
{
    uint spatial_hashes[];
};

layout(std430, binding = 6) buffer SpatialIndices
{
    uint spatial_indices[];
};

layout(std430, binding = 7) buffer SpatialOffsets
{
    uint spatial_offsets[];
};
