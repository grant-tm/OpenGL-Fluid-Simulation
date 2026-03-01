#version 430 core

uniform sampler2D u_fluid_texture;
uniform sampler2D u_depth_texture;
uniform sampler2D u_normal_texture;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

vec3 SampleEnvironment (vec3 direction)
{
    float sky_factor = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 horizon_color = vec3(0.80, 0.87, 0.97);
    vec3 zenith_color = vec3(0.16, 0.34, 0.62);
    vec3 floor_color = vec3(0.04, 0.05, 0.07);

    if (direction.y >= 0.0)
    {
        return mix(horizon_color, zenith_color, pow(sky_factor, 0.7));
    }

    return mix(horizon_color * 0.4, floor_color, clamp(-direction.y, 0.0, 1.0));
}

void main(void)
{
    vec4 packed_sample = texture(u_fluid_texture, v_texture_coordinate);
    float smooth_depth = packed_sample.r;
    float smooth_thickness = packed_sample.g;
    float hard_thickness = packed_sample.b;
    float hard_depth = packed_sample.a;

    if (smooth_thickness <= 0.010 || smooth_depth > 1000.0)
    {
        discard;
    }

    vec3 surface_normal = texture(u_normal_texture, v_texture_coordinate).xyz * 2.0 - vec3(1.0);
    if (dot(surface_normal, surface_normal) < 0.25)
    {
        discard;
    }
    surface_normal = normalize(surface_normal);

    vec2 normalized_device_coordinate = v_texture_coordinate * 2.0 - vec2(1.0);
    vec3 view_direction = normalize(vec3(normalized_device_coordinate.x, normalized_device_coordinate.y, -1.5));
    vec3 reflection_direction = reflect(view_direction, surface_normal);
    vec3 refraction_direction = refract(view_direction, surface_normal, 1.0 / 1.33);
    if (dot(refraction_direction, refraction_direction) < 0.001)
    {
        refraction_direction = view_direction;
    }

    float view_normal_alignment = clamp(dot(-view_direction, surface_normal), 0.0, 1.0);
    float fresnel_factor = 0.02 + 0.98 * pow(1.0 - view_normal_alignment, 5.0);
    vec3 extinction_coefficients = vec3(1.7, 0.85, 0.28);
    vec3 transmission_color = exp(-smooth_thickness * 2.6 * extinction_coefficients);
    vec3 reflected_color = SampleEnvironment(reflection_direction);
    vec3 refracted_color = SampleEnvironment(refraction_direction);
    vec3 water_body_color = mix(vec3(0.06, 0.18, 0.31), vec3(0.12, 0.42, 0.68), clamp(smooth_thickness * 1.2, 0.0, 1.0));
    vec3 absorbed_color = mix(water_body_color, refracted_color, transmission_color);
    vec3 final_color = mix(absorbed_color, reflected_color, fresnel_factor);

    float edge_boost = clamp((hard_thickness - smooth_thickness) * 2.2, 0.0, 1.0);
    final_color += vec3(0.12, 0.18, 0.24) * edge_boost;

    float alpha_value = clamp(smooth_thickness * 1.25 + fresnel_factor * 0.12, 0.0, 0.97);
    if (hard_depth > 1000.0)
    {
        alpha_value *= 0.0;
    }

    fragment_color = vec4(final_color, alpha_value);
}
