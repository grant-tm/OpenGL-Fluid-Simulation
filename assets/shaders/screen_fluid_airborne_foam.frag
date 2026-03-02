#version 430 core

uniform sampler2D u_fluid_texture;
uniform sampler2D u_foam_texture;
uniform sampler2D u_scene_texture;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

void main(void)
{
    vec4 packed_sample = texture(u_fluid_texture, v_texture_coordinate);
    float smooth_depth = packed_sample.r;
    vec4 foam_sample = texture(u_foam_texture, v_texture_coordinate);
    float foam = clamp(foam_sample.r, 0.0, 1.0);
    vec3 scene_color = texture(u_scene_texture, v_texture_coordinate).rgb;

    if (smooth_depth <= 1000.0)
    {
        discard;
    }

    if (foam <= 0.02)
    {
        discard;
    }

    fragment_color = vec4(scene_color * (1.0 - foam) + vec3(foam), 1.0);
}
