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
        vec4 accumulated_value =
            texture(u_source_texture, v_texture_coordinate - offset * 5.0) * 0.04000 +
            texture(u_source_texture, v_texture_coordinate - offset * 4.0) * 0.06000 +
            texture(u_source_texture, v_texture_coordinate - offset * 3.0) * 0.09000 +
            texture(u_source_texture, v_texture_coordinate - offset * 2.0) * 0.12000 +
            texture(u_source_texture, v_texture_coordinate - offset * 1.0) * 0.14000 +
            texture(u_source_texture, v_texture_coordinate) * 0.10000 +
            texture(u_source_texture, v_texture_coordinate + offset * 1.0) * 0.14000 +
            texture(u_source_texture, v_texture_coordinate + offset * 2.0) * 0.12000 +
            texture(u_source_texture, v_texture_coordinate + offset * 3.0) * 0.09000 +
            texture(u_source_texture, v_texture_coordinate + offset * 4.0) * 0.06000 +
            texture(u_source_texture, v_texture_coordinate + offset * 5.0) * 0.04000;

        fragment_color = accumulated_value;
        return;
    }

    float center_depth = texture(u_depth_texture, v_texture_coordinate).r;
    float weighted_depth_sum = 0.0;
    float total_weight = 0.0;
    float sample_weights[5] = float[5](0.22702703, 0.19459459, 0.12162162, 0.05405405, 0.01621622);

    for (int sample_offset = -4; sample_offset <= 4; sample_offset++)
    {
        vec2 sample_coordinate = v_texture_coordinate + offset * float(sample_offset);
        float sampled_depth = texture(u_source_texture, sample_coordinate).r;
        float spatial_weight = sample_weights[abs(sample_offset)];
        float depth_weight = exp(-abs(sampled_depth - center_depth) * 12.0);
        float total_sample_weight = spatial_weight * depth_weight;
        weighted_depth_sum += sampled_depth * total_sample_weight;
        total_weight += total_sample_weight;
    }

    float blurred_depth = (total_weight > 0.0) ? (weighted_depth_sum / total_weight) : center_depth;
    fragment_color = vec4(blurred_depth, 0.0, 0.0, 1.0);
}
