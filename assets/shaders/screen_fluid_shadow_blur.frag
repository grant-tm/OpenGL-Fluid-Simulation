#version 430 core

uniform sampler2D u_source_texture;
uniform vec2 u_blur_direction;
uniform vec2 u_texel_size;

in vec2 v_texture_coordinate;

layout(location = 0) out float fragment_value;

void main(void)
{
    float weights[5] = float[](0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f);
    float blurred_value = texture(u_source_texture, v_texture_coordinate).r * weights[0];

    for (int sample_index = 1; sample_index < 5; sample_index++)
    {
        vec2 sample_offset = u_blur_direction * u_texel_size * float(sample_index);
        blurred_value += texture(u_source_texture, v_texture_coordinate + sample_offset).r * weights[sample_index];
        blurred_value += texture(u_source_texture, v_texture_coordinate - sample_offset).r * weights[sample_index];
    }

    fragment_value = blurred_value;
}
