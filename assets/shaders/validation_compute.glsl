#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer ValidationData
{
    uint values[];
};

void main(void)
{
    uint index = gl_GlobalInvocationID.x;
    values[index] = index * index + 7u;
}
