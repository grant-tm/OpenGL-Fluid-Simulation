#version 430 core

in vec2 v_quad_uv;
in float v_linear_depth;

out vec4 fragment_color;

void main(void)
{
    vec2 centered_coordinate = (v_quad_uv - vec2(0.5)) * 2.0;
    float distance_squared = dot(centered_coordinate, centered_coordinate);
    if (distance_squared > 1.0)
    {
        discard;
    }

    fragment_color = vec4(1.0, gl_FragCoord.z, v_linear_depth, 1.0);
}
