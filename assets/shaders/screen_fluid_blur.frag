#version 430 core

uniform sampler2D u_source_texture;
uniform sampler2D u_depth_texture;
uniform int u_filter_mode;
uniform vec2 u_blur_direction;
uniform vec2 u_texel_size;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

void main(void)
{
    vec2 offset = u_blur_direction * u_texel_size;

    if (u_filter_mode == 0)
    {
        float center_depth = texture(u_depth_texture, v_texture_coordinate).r;
        float weighted_thickness_sum = 0.0;
        float total_weight = 0.0;
        float sample_weights[6] = float[6](0.16000, 0.14000, 0.12000, 0.09000, 0.06000, 0.04000);

        if (center_depth > 1000.0)
        {
            fragment_color = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }

        for (int sample_offset = -5; sample_offset <= 5; sample_offset++)
        {
            vec2 sample_coordinate = v_texture_coordinate + offset * float(sample_offset);
            float sample_depth = texture(u_depth_texture, sample_coordinate).r;
            float sample_thickness = texture(u_source_texture, sample_coordinate).r;
            float spatial_weight;
            float depth_difference;
            float depth_weight;
            float sample_weight;

            if (sample_depth > 1000.0)
            {
                continue;
            }

            spatial_weight = sample_weights[abs(sample_offset)];
            depth_difference = center_depth - sample_depth;
            depth_weight = exp(-(depth_difference * depth_difference) * 200.0);
            sample_weight = spatial_weight * depth_weight;
            weighted_thickness_sum += sample_thickness * sample_weight;
            total_weight += sample_weight;
        }

        if (total_weight <= 0.0)
        {
            fragment_color = vec4(0.0, 0.0, 0.0, 0.0);
            return;
        }

        float smoothed_thickness = weighted_thickness_sum / total_weight;
        fragment_color = vec4(smoothed_thickness, smoothed_thickness, smoothed_thickness, smoothed_thickness);
        return;
    }

    float center_depth = texture(u_depth_texture, v_texture_coordinate).r;
    float weighted_depth_sum = 0.0;
    float total_weight = 0.0;
    float sample_weights[5] = float[5](0.22702703, 0.19459459, 0.12162162, 0.05405405, 0.01621622);

    if (center_depth > 1000.0)
    {
        fragment_color = vec4(center_depth, 0.0, 0.0, 1.0);
        return;
    }

    for (int sample_offset = -4; sample_offset <= 4; sample_offset++)
    {
        vec2 sample_coordinate = v_texture_coordinate + offset * float(sample_offset);
        float sampled_depth = texture(u_source_texture, sample_coordinate).r;
        float spatial_weight = sample_weights[abs(sample_offset)];
        float depth_difference;
        float depth_weight;
        float total_sample_weight;

        if (sampled_depth > 1000.0)
        {
            continue;
        }

        depth_difference = center_depth - sampled_depth;
        depth_weight = exp(-(depth_difference * depth_difference) * 350.0);
        total_sample_weight = spatial_weight * depth_weight;
        weighted_depth_sum += sampled_depth * total_sample_weight;
        total_weight += total_sample_weight;
    }

    float blurred_depth = (total_weight > 0.0) ? (weighted_depth_sum / total_weight) : center_depth;
    fragment_color = vec4(blurred_depth, 0.0, 0.0, 1.0);
}
