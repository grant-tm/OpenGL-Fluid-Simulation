#version 430 core

layout(location = 0) out float fragment_depth_value;

void main(void)
{
    vec2 centered_point_coordinate = gl_PointCoord * 2.0 - vec2(1.0);
    float radial_distance_squared = dot(centered_point_coordinate, centered_point_coordinate);
    if (radial_distance_squared > 1.0)
    {
        discard;
    }

    fragment_depth_value = gl_FragCoord.z;
}
