#version 430 core

layout(location = 0) in vec4 a_position;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform float u_point_size;
uniform float u_viewport_height;

out vec3 v_view_position;
out float v_view_radius;

void main(void)
{
    vec4 view_position = u_view * vec4(a_position.xyz, 1.0);
    gl_Position = u_projection * view_position;
    gl_PointSize = u_point_size;
    v_view_position = view_position.xyz;
    v_view_radius = (u_point_size * (-view_position.z)) / (u_viewport_height * u_projection[1][1]);
}
