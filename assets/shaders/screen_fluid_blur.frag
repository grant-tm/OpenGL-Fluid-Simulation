#version 430 core

uniform sampler2D u_source_texture;
uniform vec2 u_blur_direction;
uniform vec2 u_texel_size;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

void main(void)
{
    vec2 offset = u_blur_direction * u_texel_size;

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
}
