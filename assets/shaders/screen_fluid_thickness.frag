#version 430 core

layout(location = 0) out vec4 fragment_color;

void main(void)
{
    float thickness_value = 0.10;
    fragment_color = vec4(thickness_value, thickness_value, thickness_value, thickness_value);
}
