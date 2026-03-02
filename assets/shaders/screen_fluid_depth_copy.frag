#version 430 core

uniform sampler2D u_source_texture;

in vec2 v_texture_coordinate;

void main(void)
{
    gl_FragDepth = texture(u_source_texture, v_texture_coordinate).g;
}
