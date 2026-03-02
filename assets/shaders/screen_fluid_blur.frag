#version 430 core

uniform sampler2D u_source_texture;
uniform sampler2D u_depth_texture;
uniform int u_filter_mode;
uniform vec2 u_blur_direction;
uniform vec2 u_texel_size;
uniform float u_projection_scale_x;
uniform float u_image_width;
uniform float u_world_radius;
uniform int u_max_screen_radius;
uniform float u_strength;
uniform float u_difference_strength;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

float CalculateScreenSpaceRadius (float world_radius, float linear_depth, float image_width, float projection_scale_x)
{
    float pixels_per_world_unit = (image_width * projection_scale_x) / (2.0 * linear_depth);
    return abs(pixels_per_world_unit * world_radius);
}

float CalculateGaussianWeight2D (int sample_offset_x, int sample_offset_y, float sigma)
{
    float squared_distance = float(sample_offset_x * sample_offset_x + sample_offset_y * sample_offset_y);
    return exp(-squared_distance / (2.0 * sigma * sigma));
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
    bool use_gaussian_smoothing = (u_filter_mode == 1);
    bool use_bilateral_2d_smoothing = (u_filter_mode == 2);

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
    float radius_float = CalculateScreenSpaceRadius(
        u_world_radius,
        center_hard_depth,
        u_image_width,
        u_projection_scale_x);
    if (use_bilateral_2d_smoothing)
    {
        sigma = max(0.0001, float(radius_in_pixels) * max(0.001, u_strength));

        for (int sample_offset_x = -32; sample_offset_x <= 32; sample_offset_x++)
        {
            for (int sample_offset_y = -32; sample_offset_y <= 32; sample_offset_y++)
            {
                vec2 sample_coordinate;
                vec4 sample_value;
                float sample_hard_depth;
                float spatial_weight;
                float depth_difference;
                float depth_weight;
                float total_sample_weight;

                if (abs(sample_offset_x) > radius_in_pixels || abs(sample_offset_y) > radius_in_pixels)
                {
                    continue;
                }

                sample_coordinate = v_texture_coordinate + vec2(float(sample_offset_x), float(sample_offset_y)) * u_texel_size;
                sample_value = texture(u_source_texture, sample_coordinate);
                sample_hard_depth = texture(u_depth_texture, sample_coordinate).r;

                if (sample_hard_depth > 1000.0)
                {
                    continue;
                }

                spatial_weight = CalculateGaussianWeight2D(sample_offset_x, sample_offset_y, sigma);
                depth_difference = center_hard_depth - sample_hard_depth;
                depth_weight = exp(-(depth_difference * depth_difference) * u_difference_strength);
                total_sample_weight = spatial_weight * depth_weight;
                weighted_depth_sum += sample_value.r * total_sample_weight;
                weighted_thickness_sum += sample_value.g * total_sample_weight;
                total_weight += total_sample_weight;
            }
        }
    }
    else
    {
        float fractional_radius = max(0.0, float(radius_in_pixels) - radius_float);
        sigma = max(0.0001, (float(radius_in_pixels) - fractional_radius) / (6.0 * max(0.001, u_strength)));

        for (int sample_offset = -32; sample_offset <= 32; sample_offset++)
        {
            vec2 sample_coordinate = v_texture_coordinate + offset * float(sample_offset);
            vec4 sample_value = texture(u_source_texture, sample_coordinate);
            float sample_hard_depth = texture(u_depth_texture, sample_coordinate).r;
            float spatial_weight;
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
            if (use_gaussian_smoothing)
            {
                total_sample_weight = spatial_weight;
            }
            else
            {
                float depth_difference = center_hard_depth - sample_hard_depth;
                float depth_weight = exp(-(depth_difference * depth_difference) * u_difference_strength);
                total_sample_weight = spatial_weight * depth_weight;
            }
            weighted_depth_sum += sample_value.r * total_sample_weight;
            weighted_thickness_sum += sample_value.g * total_sample_weight;
            total_weight += total_sample_weight;
        }
    }

    float blurred_depth = (total_weight > 0.0) ? (weighted_depth_sum / total_weight) : original_sample.r;
    float blurred_thickness = (total_weight > 0.0) ? (weighted_thickness_sum / total_weight) : original_sample.g;
    fragment_color = vec4(blurred_depth, blurred_thickness, original_sample.b, center_hard_depth);
}
