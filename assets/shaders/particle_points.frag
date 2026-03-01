#version 430 core

uniform vec4 u_color;

in vec4 v_color;

out vec4 fragment_color;

void main(void)
{
    if (u_color.a < 0.0)
    {
        fragment_color = u_color;
    }
    else
    {
        fragment_color = v_color;
    }
}
