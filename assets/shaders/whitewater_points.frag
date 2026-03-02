#version 430 core

in float v_linear_depth;

out vec4 fragment_color;

void main(void)
{
    vec2 centered_coordinate = gl_PointCoord * 2.0 - vec2(1.0);
    float distance_squared = dot(centered_coordinate, centered_coordinate);
    if (distance_squared > 1.0)
    {
        discard;
    }

    fragment_color = vec4(1.0, gl_FragCoord.z, v_linear_depth, 1.0);
}
