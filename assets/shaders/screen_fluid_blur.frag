#version 430 core

uniform sampler2D u_source_texture;
uniform sampler2D u_depth_texture;
uniform vec2 u_blur_direction;
uniform vec2 u_texel_size;
uniform float u_projection_scale_x;
uniform float u_image_width;
uniform float u_world_radius;
uniform int u_max_screen_radius;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

float CalculateScreenSpaceRadius (float world_radius, float linear_depth, float image_width, float projection_scale_x)
{
    float pixels_per_world_unit = (image_width * projection_scale_x) / (2.0 * linear_depth);
    return abs(pixels_per_world_unit * world_radius);
}

void main(void)
{
    vec4 original_sample = texture(u_source_texture, v_texture_coordinate);
    float center_hard_depth = texture(u_depth_texture, v_texture_coordinate).r;
    vec2 offset = u_blur_direction * u_texel_size;
    float weighted_depth_sum = 0.0;
    float weighted_thickness_sum = 0.0;
    float total_weight = 0.0;
    int radius_in_pixels;
    float sigma;

    if (center_hard_depth > 1000.0)
    {
        fragment_color = vec4(10000.0, 0.0, 0.0, 10000.0);
        return;
    }

    radius_in_pixels = int(ceil(CalculateScreenSpaceRadius(
        u_world_radius,
        center_hard_depth,
        u_image_width,
        u_projection_scale_x)));
    if (radius_in_pixels < 2) radius_in_pixels = 2;
    if (radius_in_pixels > u_max_screen_radius) radius_in_pixels = u_max_screen_radius;
    sigma = max(0.75, (float(radius_in_pixels) / 3.0));

    for (int sample_offset = -12; sample_offset <= 12; sample_offset++)
    {
        vec2 sample_coordinate = v_texture_coordinate + offset * float(sample_offset);
        vec4 sample_value = texture(u_source_texture, sample_coordinate);
        float sample_hard_depth = texture(u_depth_texture, sample_coordinate).r;
        float spatial_weight;
        float depth_difference;
        float depth_weight;
        float total_sample_weight;

        if (abs(sample_offset) > radius_in_pixels)
        {
            continue;
        }

        if (sample_hard_depth > 1000.0)
        {
            continue;
        }

        spatial_weight = exp(-(float(sample_offset * sample_offset)) / (2.0 * sigma * sigma));
        depth_difference = center_hard_depth - sample_hard_depth;
        depth_weight = exp(-(depth_difference * depth_difference) * 140.0);
        total_sample_weight = spatial_weight * depth_weight;
        weighted_depth_sum += sample_value.r * total_sample_weight;
        weighted_thickness_sum += sample_value.g * total_sample_weight;
        total_weight += total_sample_weight;
    }

    float blurred_depth = (total_weight > 0.0) ? (weighted_depth_sum / total_weight) : original_sample.r;
    float blurred_thickness = (total_weight > 0.0) ? (weighted_thickness_sum / total_weight) : original_sample.g;
    fragment_color = vec4(blurred_depth, blurred_thickness, original_sample.b, center_hard_depth);
}
