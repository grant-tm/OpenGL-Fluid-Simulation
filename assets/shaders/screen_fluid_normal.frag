#version 430 core

uniform sampler2D u_comp_texture;
uniform vec2 u_texel_size;
uniform mat4 u_projection;
uniform mat4 u_inverse_projection;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

vec3 ViewPositionFromLinearDepth (vec2 sample_texture_coordinate, float linear_depth)
{
    vec2 normalized_device_coordinate = sample_texture_coordinate * 2.0 - vec2(1.0);
    vec3 view_vector = (u_inverse_projection * vec4(normalized_device_coordinate, 0.0, -1.0)).xyz;
    vec3 ray_direction = normalize(view_vector);
    return ray_direction * linear_depth;
}

void main(void)
{
    vec4 center_sample = texture(u_comp_texture, v_texture_coordinate);
    float center_depth = center_sample.r;

    if (center_depth > 1000.0)
    {
        fragment_color = vec4(0.5, 0.5, 1.0, 0.0);
        return;
    }

    vec2 right_offset = vec2(u_texel_size.x, 0.0);
    vec2 up_offset = vec2(0.0, u_texel_size.y);
    vec3 center_view_position = ViewPositionFromLinearDepth(v_texture_coordinate, center_depth);

    float right_depth = texture(u_comp_texture, v_texture_coordinate + right_offset).r;
    float left_depth = texture(u_comp_texture, v_texture_coordinate - right_offset).r;
    float up_depth = texture(u_comp_texture, v_texture_coordinate + up_offset).r;
    float down_depth = texture(u_comp_texture, v_texture_coordinate - up_offset).r;

    vec3 ddx_forward = ViewPositionFromLinearDepth(v_texture_coordinate + right_offset, right_depth) - center_view_position;
    vec3 ddx_backward = center_view_position - ViewPositionFromLinearDepth(v_texture_coordinate - right_offset, left_depth);
    vec3 ddy_forward = ViewPositionFromLinearDepth(v_texture_coordinate + up_offset, up_depth) - center_view_position;
    vec3 ddy_backward = center_view_position - ViewPositionFromLinearDepth(v_texture_coordinate - up_offset, down_depth);

    vec3 ddx = (abs(ddx_backward.z) < abs(ddx_forward.z)) ? ddx_backward : ddx_forward;
    vec3 ddy = (abs(ddy_backward.z) < abs(ddy_forward.z)) ? ddy_backward : ddy_forward;
    vec3 view_normal = normalize(cross(ddy, ddx));

    fragment_color = vec4(view_normal * 0.5 + vec3(0.5), 1.0);
}
