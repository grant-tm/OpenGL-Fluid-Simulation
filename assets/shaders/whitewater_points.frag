#version 430 core

in vec2 v_quad_uv;
in float v_linear_depth;
in float v_surface_depth;
in float v_neighbor_count;

uniform int u_debug_output_mode;

out vec4 fragment_color;

void main(void)
{
    vec2 centered_coordinate = (v_quad_uv - vec2(0.5)) * 2.0;
    float distance_squared = dot(centered_coordinate, centered_coordinate);
    if (distance_squared > 1.0)
    {
        discard;
    }

    if (u_debug_output_mode == 1)
    {
        float neighbor_count_value = clamp(v_neighbor_count / 20.0, 0.0, 1.0);
        fragment_color = vec4(neighbor_count_value, v_surface_depth, v_linear_depth, 1.0);
        return;
    }

    fragment_color = vec4(1.0, v_surface_depth, v_linear_depth, 1.0);
}
