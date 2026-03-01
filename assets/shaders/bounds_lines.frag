#version 430 core

uniform vec4 u_color;

out vec4 fragment_color;

void main(void)
{
    fragment_color = u_color;
}
