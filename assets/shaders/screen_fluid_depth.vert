#version 430 core

layout(location = 0) in vec4 a_position;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform float u_point_size;

void main(void)
{
    vec4 view_position = u_view * vec4(a_position.xyz, 1.0);
    gl_Position = u_projection * view_position;
    gl_PointSize = u_point_size;
}
