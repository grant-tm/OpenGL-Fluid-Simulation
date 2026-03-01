#version 430 core

uniform sampler3D u_density_texture;
uniform vec3 u_bounds_size;
uniform vec3 u_camera_position;
uniform int u_step_count;
uniform float u_density_multiplier;
uniform vec3 u_voxel_size;

in vec3 v_local_position;
in vec3 v_world_position;

out vec4 fragment_color;

bool RayBoxIntersection (
    vec3 ray_origin,
    vec3 ray_direction,
    out float hit_t_minimum,
    out float hit_t_maximum)
{
    vec3 box_minimum = vec3(-0.5);
    vec3 box_maximum = vec3(0.5);
    vec3 inverse_direction = 1.0 / max(abs(ray_direction), vec3(0.00001)) * sign(ray_direction);

    vec3 t0 = (box_minimum - ray_origin) * inverse_direction;
    vec3 t1 = (box_maximum - ray_origin) * inverse_direction;
    vec3 t_minimum = min(t0, t1);
    vec3 t_maximum = max(t0, t1);

    hit_t_minimum = max(max(t_minimum.x, t_minimum.y), t_minimum.z);
    hit_t_maximum = min(min(t_maximum.x, t_maximum.y), t_maximum.z);
    return hit_t_maximum > max(hit_t_minimum, 0.0);
}

float SampleDensity (vec3 local_position)
{
    vec3 texture_coordinate = local_position + vec3(0.5);
    return texture(u_density_texture, texture_coordinate).r;
}

vec3 DensityColor (float density_value)
{
    vec3 low_color = vec3(0.04, 0.12, 0.34);
    vec3 mid_color = vec3(0.10, 0.48, 0.82);
    vec3 high_color = vec3(0.88, 0.96, 1.0);
    float middle_mix = clamp(density_value * 0.12, 0.0, 1.0);
    vec3 mixed_color = mix(low_color, mid_color, middle_mix);
    return mix(mixed_color, high_color, clamp(density_value * 0.06, 0.0, 1.0));
}

vec3 EstimateDensityNormal (vec3 local_position)
{
    vec3 center_texture_coordinate = local_position + vec3(0.5);
    vec3 offset_x = vec3(u_voxel_size.x, 0.0, 0.0);
    vec3 offset_y = vec3(0.0, u_voxel_size.y, 0.0);
    vec3 offset_z = vec3(0.0, 0.0, u_voxel_size.z);

    float density_x0 = texture(u_density_texture, center_texture_coordinate - offset_x).r;
    float density_x1 = texture(u_density_texture, center_texture_coordinate + offset_x).r;
    float density_y0 = texture(u_density_texture, center_texture_coordinate - offset_y).r;
    float density_y1 = texture(u_density_texture, center_texture_coordinate + offset_y).r;
    float density_z0 = texture(u_density_texture, center_texture_coordinate - offset_z).r;
    float density_z1 = texture(u_density_texture, center_texture_coordinate + offset_z).r;

    vec3 gradient = vec3(density_x1 - density_x0, density_y1 - density_y0, density_z1 - density_z0);
    float gradient_length = length(gradient);
    if (gradient_length <= 0.00001)
    {
        return vec3(0.0, 1.0, 0.0);
    }

    return normalize(gradient);
}

float InterleavedGradientNoise (vec2 screen_position)
{
    return fract(52.9829189 * fract(dot(screen_position, vec2(0.06711056, 0.00583715))));
}

void main(void)
{
    vec3 camera_local_position = u_camera_position / u_bounds_size;
    vec3 fragment_local_position = v_world_position / u_bounds_size;
    vec3 ray_direction = normalize(fragment_local_position - camera_local_position);

    float hit_t_minimum = 0.0;
    float hit_t_maximum = 0.0;
    if (!RayBoxIntersection(camera_local_position, ray_direction, hit_t_minimum, hit_t_maximum))
    {
        discard;
    }

    hit_t_minimum = max(hit_t_minimum, 0.0);
    float ray_length = hit_t_maximum - hit_t_minimum;
    if (ray_length <= 0.0)
    {
        discard;
    }

    int step_count = max(u_step_count, 8);
    float step_size = ray_length / float(step_count);
    float jitter = InterleavedGradientNoise(gl_FragCoord.xy) - 0.5;
    vec3 current_position = camera_local_position + ray_direction * (hit_t_minimum + step_size * (0.5 + jitter * 0.65));

    vec3 accumulated_color = vec3(0.0);
    float accumulated_alpha = 0.0;
    vec3 light_direction = normalize(vec3(-0.45, 0.75, 0.48));
    vec3 view_direction = normalize(-ray_direction);

    for (int step_index = 0; step_index < step_count; step_index++)
    {
        float density_value = SampleDensity(current_position) * u_density_multiplier;
        float opacity = 1.0 - exp(-density_value * 1.2);
        opacity = clamp(opacity, 0.0, 0.25);
        vec3 sample_color = DensityColor(density_value);
        vec3 normal = EstimateDensityNormal(current_position);
        float diffuse = max(dot(normal, light_direction), 0.0);
        float rim = pow(1.0 - max(dot(normal, view_direction), 0.0), 2.0);
        float forward_scatter = pow(max(dot(light_direction, view_direction), 0.0), 6.0);
        sample_color *= 0.42 + diffuse * 0.85;
        sample_color += vec3(0.10, 0.16, 0.24) * rim;
        sample_color += vec3(0.18, 0.24, 0.30) * forward_scatter * opacity;

        accumulated_color += (1.0 - accumulated_alpha) * sample_color * opacity;
        accumulated_alpha += (1.0 - accumulated_alpha) * opacity;

        if (accumulated_alpha >= 0.98)
        {
            break;
        }

        current_position += ray_direction * step_size;
    }

    if (accumulated_alpha <= 0.001)
    {
        discard;
    }

    accumulated_color = pow(accumulated_color, vec3(0.92));
    fragment_color = vec4(accumulated_color, accumulated_alpha * 0.96);
}
