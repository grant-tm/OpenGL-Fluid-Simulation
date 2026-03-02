#version 430 core

uniform sampler2D u_debug_texture;
uniform int u_debug_mode;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

void main(void)
{
    vec4 sample_value = texture(u_debug_texture, v_texture_coordinate);

    if (u_debug_mode == 1)
    {
        float smooth_depth = sample_value.r;
        float smooth_thickness = sample_value.g;
        float hard_thickness = sample_value.b;
        float hard_depth = sample_value.a;

        if (hard_depth > 1000.0)
        {
            fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        float normalized_depth = clamp((smooth_depth - 2.0) / 7.0, 0.0, 1.0);
        float depth_display = 1.0 - normalized_depth;
        float smooth_thickness_display = clamp(smooth_thickness * 8.0, 0.0, 1.0);
        float hard_thickness_display = clamp(hard_thickness * 10.0, 0.0, 1.0);
        fragment_color = vec4(depth_display, smooth_thickness_display, hard_thickness_display, 1.0);
        return;
    }

    if (u_debug_mode == 2)
    {
        fragment_color = vec4(sample_value.rgb, 1.0);
        return;
    }

    fragment_color = vec4(1.0, 0.0, 1.0, 1.0);
}
