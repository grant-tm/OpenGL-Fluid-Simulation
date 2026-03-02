#version 430 core

uniform mat4 u_projection;

in vec2 v_quad_uv;
in vec3 v_view_center_position;
in float v_world_radius;

layout(location = 0) out float fragment_depth_value;

void main(void)
{
    vec2 centered_point_coordinate = (v_quad_uv - vec2(0.5)) * 2.0;
    float radial_distance_squared = dot(centered_point_coordinate, centered_point_coordinate);
    if (radial_distance_squared > 1.0)
    {
        discard;
    }

    float sphere_z_offset = sqrt(max(0.0, 1.0 - radial_distance_squared)) * v_world_radius;
    vec3 fragment_view_position = vec3(
        v_view_center_position.x + centered_point_coordinate.x * v_world_radius,
        v_view_center_position.y + centered_point_coordinate.y * v_world_radius,
        v_view_center_position.z + sphere_z_offset);
    vec4 fragment_clip_position = u_projection * vec4(fragment_view_position, 1.0);
    float normalized_device_depth = fragment_clip_position.z / fragment_clip_position.w;
    gl_FragDepth = normalized_device_depth * 0.5 + 0.5;
    fragment_depth_value = length(fragment_view_position);
}
