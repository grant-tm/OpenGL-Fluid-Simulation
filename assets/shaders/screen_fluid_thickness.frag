#version 430 core

in vec2 v_quad_uv;

layout(location = 0) out vec4 fragment_color;

void main(void)
{
    vec2 centered_point_coordinate = (v_quad_uv - vec2(0.5)) * 2.0;
    float radial_distance_squared = dot(centered_point_coordinate, centered_point_coordinate);
    if (radial_distance_squared > 1.0)
    {
        discard;
    }

    float thickness_value = 0.10;
    fragment_color = vec4(thickness_value, thickness_value, thickness_value, thickness_value);
}
