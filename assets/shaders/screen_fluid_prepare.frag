#version 430 core

uniform sampler2D u_depth_texture;
uniform sampler2D u_thickness_texture;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

void main(void)
{
    float hard_depth_value = texture(u_depth_texture, v_texture_coordinate).r;
    float hard_thickness_value = texture(u_thickness_texture, v_texture_coordinate).r;

    fragment_color = vec4(hard_depth_value, hard_thickness_value, hard_thickness_value, hard_depth_value);
}
