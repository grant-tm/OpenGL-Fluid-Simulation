#version 430 core

layout(location = 0) in vec2 a_quad_offset;
layout(location = 1) in vec2 a_quad_uv;
layout(location = 2) in vec4 a_position_and_scale;
layout(location = 3) in vec4 a_velocity_and_lifetime;
layout(location = 4) in vec4 a_classification_data;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform float u_scale;
uniform int u_debug_class_filter;
uniform int u_debug_output_mode;

out vec2 v_quad_uv;
out float v_linear_depth;
out float v_surface_depth;
out float v_neighbor_count;
out float v_lifetime;

float Remap01(float value, float minimum_value, float maximum_value)
{
    float denominator = maximum_value - minimum_value;
    if (abs(denominator) < 0.0001) return 0.0;
    return clamp((value - minimum_value) / denominator, 0.0, 1.0);
}

void main(void)
{
    vec3 position = a_position_and_scale.xyz;
    float particle_scale = a_position_and_scale.w;
    vec3 velocity = a_velocity_and_lifetime.xyz;
    float lifetime = a_velocity_and_lifetime.w;
    int particle_class = int(round(a_classification_data.x));

    if (lifetime <= 0.0 || particle_scale <= 0.0 ||
        (u_debug_class_filter != 0 && particle_class != u_debug_class_filter))
    {
        gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
        v_quad_uv = a_quad_uv;
        v_linear_depth = 10000.0;
        v_surface_depth = 1.0;
        v_neighbor_count = 0.0;
        v_lifetime = 0.0;
        return;
    }

    float dissolve_scale = clamp(lifetime / 3.0, 0.0, 1.0);
    float speed = length(velocity);
    float velocity_scale = mix(0.6, 1.0, Remap01(speed, 1.0, 3.0));
    float billboard_scale = u_scale * 2.0 * dissolve_scale * particle_scale * velocity_scale;
    vec3 camera_up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
    vec3 camera_right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 world_position =
        position +
        camera_right * (a_quad_offset.x * billboard_scale) +
        camera_up * (a_quad_offset.y * billboard_scale);
    vec3 view_position = (u_view * vec4(world_position, 1.0)).xyz;
    vec3 center_view_position = (u_view * vec4(position, 1.0)).xyz;
    vec4 center_clip_position = u_projection * vec4(center_view_position, 1.0);

    gl_Position = u_projection * vec4(view_position, 1.0);
    v_quad_uv = a_quad_uv;
    v_linear_depth = abs(center_view_position.z);
    v_surface_depth = center_clip_position.z / center_clip_position.w * 0.5 + 0.5;
    v_neighbor_count = a_classification_data.y;
    v_lifetime = lifetime;
}
