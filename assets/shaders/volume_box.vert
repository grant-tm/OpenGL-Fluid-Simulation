#version 430 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform vec3 u_bounds_size;

out vec3 v_local_position;
out vec3 v_world_position;

void main(void)
{
    v_local_position = a_position;
    v_world_position = a_position * u_bounds_size;
    gl_Position = u_projection * u_view * vec4(v_world_position, 1.0);
}
