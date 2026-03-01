#version 430 core

uniform sampler2D u_fluid_texture;
uniform sampler2D u_depth_texture;
uniform vec2 u_texel_size;
uniform mat4 u_projection;

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

vec3 ViewPositionFromLinearDepth (vec2 sample_texture_coordinate, float linear_depth)
{
    vec2 normalized_device_coordinate = sample_texture_coordinate * 2.0 - vec2(1.0);

    return vec3(
        normalized_device_coordinate.x * linear_depth / u_projection[0][0],
        normalized_device_coordinate.y * linear_depth / u_projection[1][1],
        -linear_depth);
}

void main(void)
{
    float center_thickness = SampleThickness(v_texture_coordinate);
    float center_depth = SampleDepth(v_texture_coordinate);
    if (center_thickness <= 0.010 || center_depth > 1000.0)
    {
        discard;
    }

    vec2 right_offset = vec2(u_texel_size.x, 0.0);
    vec2 up_offset = vec2(0.0, u_texel_size.y);
    vec3 center_view_position = ViewPositionFromLinearDepth(v_texture_coordinate, center_depth);
    float depth_right = SampleDepth(v_texture_coordinate + right_offset);
    float depth_left = SampleDepth(v_texture_coordinate - right_offset);
    float depth_up = SampleDepth(v_texture_coordinate + up_offset);
    float depth_down = SampleDepth(v_texture_coordinate - up_offset);
    vec3 ddx_forward = ViewPositionFromLinearDepth(v_texture_coordinate + right_offset, depth_right) - center_view_position;
    vec3 ddx_backward = center_view_position - ViewPositionFromLinearDepth(v_texture_coordinate - right_offset, depth_left);
    vec3 ddy_forward = ViewPositionFromLinearDepth(v_texture_coordinate + up_offset, depth_up) - center_view_position;
    vec3 ddy_backward = center_view_position - ViewPositionFromLinearDepth(v_texture_coordinate - up_offset, depth_down);
    vec3 ddx = (abs(ddx_backward.z) < abs(ddx_forward.z)) ? ddx_backward : ddx_forward;
    vec3 ddy = (abs(ddy_backward.z) < abs(ddy_forward.z)) ? ddy_backward : ddy_forward;
    vec3 surface_normal = normalize(cross(ddy, ddx));
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
