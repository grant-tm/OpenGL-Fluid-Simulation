#version 430 core

layout(location = 0) out vec4 fragment_color;

void main(void)
{
    vec2 centered_point_coordinate = gl_PointCoord * 2.0 - vec2(1.0);
    float radial_distance_squared = dot(centered_point_coordinate, centered_point_coordinate);
    float thickness_value = max(0.0, 1.0 - radial_distance_squared) * 0.10;
    fragment_color = vec4(thickness_value, thickness_value, thickness_value, thickness_value);
}
