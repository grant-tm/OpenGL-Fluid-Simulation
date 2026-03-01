#version 430 core

layout(location = 0) in vec2 a_quad_offset;
layout(location = 1) in vec2 a_quad_uv;
layout(location = 2) in vec4 a_particle_position;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform float u_point_size;

out vec2 v_quad_uv;
out vec3 v_view_center_position;
out float v_world_radius;

void main(void)
{
    vec3 camera_right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 camera_up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
    vec3 world_position =
        a_particle_position.xyz +
        camera_right * (a_quad_offset.x * u_point_size * 2.0) +
        camera_up * (a_quad_offset.y * u_point_size * 2.0);
    vec4 view_position = u_view * vec4(world_position, 1.0);
    vec4 view_center_position = u_view * vec4(a_particle_position.xyz, 1.0);

    v_quad_uv = a_quad_uv;
    v_view_center_position = view_center_position.xyz;
    v_world_radius = u_point_size;
    gl_Position = u_projection * view_position;
}
