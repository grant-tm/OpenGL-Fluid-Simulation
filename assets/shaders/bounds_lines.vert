#version 430 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

void main(void)
{
    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
}
