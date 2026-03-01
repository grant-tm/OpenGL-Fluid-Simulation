#version 430 core

uniform sampler2D u_fluid_texture;
uniform sampler2D u_depth_texture;
uniform vec2 u_texel_size;

in vec2 v_texture_coordinate;

out vec4 fragment_color;

float SampleThickness (vec2 sample_texture_coordinate)
{
    vec4 fluid_sample = texture(u_fluid_texture, sample_texture_coordinate);
    return max(max(fluid_sample.r, fluid_sample.g), max(fluid_sample.b, fluid_sample.a));
}

float SampleDepth (vec2 sample_texture_coordinate)
{
    return texture(u_depth_texture, sample_texture_coordinate).r;
}

void main(void)
{
    float center_thickness = SampleThickness(v_texture_coordinate);
    float center_depth = SampleDepth(v_texture_coordinate);
    if (center_thickness <= 0.010 || center_depth >= 0.9999)
    {
        discard;
    }

    float depth_left = SampleDepth(v_texture_coordinate - vec2(u_texel_size.x, 0.0));
    float depth_right = SampleDepth(v_texture_coordinate + vec2(u_texel_size.x, 0.0));
    float depth_down = SampleDepth(v_texture_coordinate - vec2(0.0, u_texel_size.y));
    float depth_up = SampleDepth(v_texture_coordinate + vec2(0.0, u_texel_size.y));
    vec2 depth_gradient = vec2(depth_right - depth_left, depth_up - depth_down);
    vec3 surface_normal = normalize(vec3(-depth_gradient.x * 90.0, -depth_gradient.y * 90.0, 1.0));
    vec3 light_direction = normalize(vec3(-0.45, 0.70, 0.55));
    vec3 view_direction = vec3(0.0, 0.0, 1.0);
    vec3 half_vector = normalize(light_direction + view_direction);

    float diffuse_light = max(dot(surface_normal, light_direction), 0.0);
    float rim_light = pow(1.0 - max(dot(surface_normal, view_direction), 0.0), 3.0);
    float specular_light = pow(max(dot(surface_normal, half_vector), 0.0), 24.0);
    float body_factor = clamp(center_thickness * 1.8, 0.0, 1.0);

    vec3 deep_color = vec3(0.03, 0.11, 0.23);
    vec3 shallow_color = vec3(0.14, 0.46, 0.78);
    vec3 highlight_color = vec3(0.92, 0.98, 1.0);
    vec3 base_color = mix(deep_color, shallow_color, body_factor);
    vec3 shaded_color = base_color * (0.50 + diffuse_light * 0.65);
    shaded_color += highlight_color * (specular_light * 0.35 + rim_light * 0.06);

    float alpha_value = clamp(center_thickness * 1.10 + rim_light * 0.04, 0.0, 0.88);
    fragment_color = vec4(shaded_color, alpha_value);
}
