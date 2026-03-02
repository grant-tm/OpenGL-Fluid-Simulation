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

vec3 SelectStableDerivative(vec2 sample_texture_coordinate, vec3 center_view_position, vec2 axis_offset)
{
    float forward_depth_near = texture(u_comp_texture, sample_texture_coordinate + axis_offset).r;
    float backward_depth_near = texture(u_comp_texture, sample_texture_coordinate - axis_offset).r;
    float forward_depth_far = texture(u_comp_texture, sample_texture_coordinate + axis_offset * 2.0).r;
    float backward_depth_far = texture(u_comp_texture, sample_texture_coordinate - axis_offset * 2.0).r;

    vec3 forward_near =
        ViewPositionFromLinearDepth(sample_texture_coordinate + axis_offset, forward_depth_near) -
        center_view_position;
    vec3 backward_near =
        center_view_position -
        ViewPositionFromLinearDepth(sample_texture_coordinate - axis_offset, backward_depth_near);
    vec3 forward_far =
        ViewPositionFromLinearDepth(sample_texture_coordinate + axis_offset * 2.0, forward_depth_far) -
        center_view_position;
    vec3 backward_far =
        center_view_position -
        ViewPositionFromLinearDepth(sample_texture_coordinate - axis_offset * 2.0, backward_depth_far);

    vec3 near_derivative = abs(backward_near.z) < abs(forward_near.z) ? backward_near : forward_near;
    vec3 far_derivative = abs(backward_far.z) < abs(forward_far.z) ? backward_far : forward_far;

    return normalize(near_derivative) * 0.7 + normalize(far_derivative) * 0.3;
}

vec3 ComputeViewNormalAt(vec2 sample_texture_coordinate, float center_depth)
{
    vec3 center_view_position = ViewPositionFromLinearDepth(sample_texture_coordinate, center_depth);
    vec3 ddx = SelectStableDerivative(sample_texture_coordinate, center_view_position, vec2(u_texel_size.x, 0.0));
    vec3 ddy = SelectStableDerivative(sample_texture_coordinate, center_view_position, vec2(0.0, u_texel_size.y));
    return normalize(cross(ddy, ddx));
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

    vec2 neighbor_offsets[4] = vec2[](
        vec2(u_texel_size.x, 0.0),
        vec2(-u_texel_size.x, 0.0),
        vec2(0.0, u_texel_size.y),
        vec2(0.0, -u_texel_size.y)
    );

    vec3 accumulated_normal = ComputeViewNormalAt(v_texture_coordinate, center_depth);
    float accumulated_weight = 1.0;

    for (int neighbor_index = 0; neighbor_index < 4; neighbor_index++)
    {
        vec2 sample_texture_coordinate = v_texture_coordinate + neighbor_offsets[neighbor_index];
        float sample_depth = texture(u_comp_texture, sample_texture_coordinate).r;
        if (sample_depth > 1000.0)
        {
            continue;
        }

        float depth_difference = abs(sample_depth - center_depth);
        float sample_weight = exp(-depth_difference * 18.0);
        vec3 sample_normal = ComputeViewNormalAt(sample_texture_coordinate, sample_depth);
        accumulated_normal += sample_normal * sample_weight;
        accumulated_weight += sample_weight;
    }

    vec3 view_normal = normalize(accumulated_normal / accumulated_weight);

    fragment_color = vec4(view_normal * 0.5 + vec3(0.5), 1.0);
}
