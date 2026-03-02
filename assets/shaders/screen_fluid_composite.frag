#version 430 core

uniform sampler2D u_fluid_texture;
uniform sampler2D u_depth_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_scene_texture;
uniform sampler2D u_foam_texture;
uniform sampler2D u_shadow_texture;
uniform mat4 u_projection;
uniform mat4 u_inverse_projection;
uniform mat4 u_inverse_view;
uniform mat4 u_shadow_view_projection;
uniform vec3 u_bounds_size;

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

vec3 ViewPositionFromLinearDepth (vec2 sample_texture_coordinate, float linear_depth)
{
    vec2 normalized_device_coordinate = sample_texture_coordinate * 2.0 - vec2(1.0);
    vec3 view_vector = (u_inverse_projection * vec4(normalized_device_coordinate, 0.0, -1.0)).xyz;
    vec3 ray_direction = normalize(view_vector);
    return ray_direction * linear_depth;
}

vec3 WorldPositionFromViewPosition (vec3 view_position)
{
    return (u_inverse_view * vec4(view_position, 1.0)).xyz;
}

vec3 ViewDirectionToWorldDirection (vec3 view_direction)
{
    return normalize((u_inverse_view * vec4(view_direction, 0.0)).xyz);
}

vec3 CalculateClosestFaceNormal (vec3 box_size, vec3 position)
{
    vec3 half_size = box_size * 0.5;
    vec3 edge_offset = half_size - abs(position);

    if (edge_offset.x < edge_offset.y && edge_offset.x < edge_offset.z)
    {
        return vec3(sign(position.x), 0.0, 0.0);
    }
    if (edge_offset.y < edge_offset.z)
    {
        return vec3(0.0, sign(position.y), 0.0);
    }
    return vec3(0.0, 0.0, sign(position.z));
}

vec3 SmoothEdgeNormal (vec3 world_normal, vec3 world_position, vec3 box_size)
{
    vec3 edge_offset = box_size * 0.5 - abs(world_position);
    float face_weight = max(0.0, min(edge_offset.x, edge_offset.z));
    vec3 face_normal = CalculateClosestFaceNormal(box_size, world_position);
    float corner_weight = 1.0 - clamp(abs(edge_offset.x - edge_offset.z) * 6.0, 0.0, 1.0);
    face_weight = 1.0 - smoothstep(0.0, 0.05, face_weight);
    face_weight *= (1.0 - corner_weight);

    return normalize(mix(world_normal, face_normal, face_weight));
}

void main(void)
{
    vec4 packed_sample = texture(u_fluid_texture, v_texture_coordinate);
    float smooth_depth = packed_sample.r;
    float smooth_thickness = packed_sample.g;
    float hard_thickness = packed_sample.b;
    float hard_depth = packed_sample.a;
    vec4 foam_sample = texture(u_foam_texture, v_texture_coordinate);
    float foam = foam_sample.r;
    float foam_depth = foam_sample.b;
    float foam_surface_visibility = 0.0;

    if (smooth_depth > 1000.0)
    {
        discard;
    }

    if (smooth_thickness <= 0.010)
    {
        discard;
    }

    if (smooth_thickness + hard_thickness * 0.35 <= 0.030)
    {
        discard;
    }

    foam_surface_visibility =
        foam *
        (
            1.0 -
            smoothstep(
                hard_depth + 0.010,
                hard_depth + 0.120,
                foam_depth));

    vec3 surface_normal = texture(u_normal_texture, v_texture_coordinate).xyz * 2.0 - vec3(1.0);
    if (dot(surface_normal, surface_normal) < 0.25)
    {
        discard;
    }
    surface_normal = normalize(surface_normal);

    vec2 normalized_device_coordinate = v_texture_coordinate * 2.0 - vec2(1.0);
    vec3 view_position = ViewPositionFromLinearDepth(v_texture_coordinate, smooth_depth);
    vec3 world_position = WorldPositionFromViewPosition(view_position);
    vec3 world_normal = ViewDirectionToWorldDirection(surface_normal);

    vec3 view_direction = normalize(view_position);
    vec3 world_view_direction = ViewDirectionToWorldDirection(view_direction);
    vec3 incident_direction = normalize(-world_view_direction);
    vec3 reflection_direction = reflect(incident_direction, world_normal);
    vec3 refraction_direction = refract(incident_direction, world_normal, 1.0 / 1.333);
    if (dot(refraction_direction, refraction_direction) < 0.001)
    {
        refraction_direction = incident_direction;
    }

    float view_normal_alignment = clamp(dot(incident_direction, world_normal), 0.0, 1.0);
    float fresnel_factor = 0.02 + 0.98 * pow(1.0 - view_normal_alignment, 5.0);

    float refraction_offset_scale = 0.055;
    vec2 refraction_offset = world_normal.xy * smooth_thickness * refraction_offset_scale;
    vec2 refracted_texture_coordinate = clamp(v_texture_coordinate + refraction_offset, vec2(0.001), vec2(0.999));
    vec3 scene_color = texture(u_scene_texture, refracted_texture_coordinate).rgb;
    vec3 fallback_scene_color = mix(
        vec3(0.10, 0.15, 0.22),
        vec3(0.42, 0.58, 0.74),
        clamp(refracted_texture_coordinate.y * 0.85 + 0.15, 0.0, 1.0));
    scene_color = mix(fallback_scene_color, scene_color, 0.55);

    vec3 reflected_color = SampleEnvironment(reflection_direction);
    vec3 refracted_environment_color = SampleEnvironment(refraction_direction);
    vec3 extinction_coefficients = vec3(1.9, 0.95, 0.30);
    vec3 transmission_factor = exp(-smooth_thickness * 3.2 * extinction_coefficients);
    vec3 deep_water_color = vec3(0.05, 0.20, 0.34);
    vec3 shallow_water_color = vec3(0.20, 0.52, 0.78);
    vec3 in_scatter_color = mix(deep_water_color, shallow_water_color, clamp(smooth_thickness * 0.9, 0.0, 1.0));
    vec3 transmitted_scene_color = scene_color * transmission_factor;
    vec3 refracted_color = mix(in_scatter_color, transmitted_scene_color, clamp(transmission_factor, vec3(0.0), vec3(1.0)));
    refracted_color = mix(refracted_color, refracted_environment_color, 0.24);
    refracted_color = refracted_color * (1.0 - foam_surface_visibility) + vec3(foam_surface_visibility);

    vec4 shadow_clip_position = u_shadow_view_projection * vec4(world_position, 1.0);
    shadow_clip_position /= shadow_clip_position.w;
    vec2 shadow_texture_coordinate = shadow_clip_position.xy * 0.5 + vec2(0.5);
    float shadow_edge_weight =
        shadow_texture_coordinate.x >= 0.0 && shadow_texture_coordinate.x <= 1.0 &&
        shadow_texture_coordinate.y >= 0.0 && shadow_texture_coordinate.y <= 1.0 ? 1.0 : 0.0;
    float shadow_thickness = texture(u_shadow_texture, shadow_texture_coordinate).r * shadow_edge_weight;
    vec3 shadow_transmission = exp(-shadow_thickness * vec3(1.9, 0.95, 0.30));
    shadow_transmission = shadow_transmission * 0.83 + vec3(0.17);
    refracted_color *= shadow_transmission;

    vec3 light_direction = normalize(vec3(-0.45, 0.72, 0.52));
    vec3 half_vector = normalize(light_direction + incident_direction);
    float diffuse_light = max(dot(world_normal, light_direction), 0.0);
    float specular_light = pow(max(dot(world_normal, half_vector), 0.0), 96.0);
    float grazing_light = pow(1.0 - view_normal_alignment, 2.5);

    reflected_color = max(reflected_color, vec3(0.18, 0.22, 0.28));
    reflected_color = reflected_color * (1.0 - foam_surface_visibility) + vec3(foam_surface_visibility);
    vec3 final_color = mix(refracted_color, reflected_color, fresnel_factor);
    final_color += vec3(0.85, 0.93, 1.0) * specular_light * 0.35;
    final_color += vec3(0.06, 0.10, 0.14) * diffuse_light * grazing_light;
    final_color += shallow_water_color * (0.18 + smooth_thickness * 0.10);

    float edge_foam_factor = clamp((hard_thickness - smooth_thickness) * 2.8, 0.0, 1.0);
    final_color += vec3(0.08, 0.10, 0.12) * edge_foam_factor;
    final_color = max(final_color, deep_water_color * 0.85);

    if (hard_depth > 1000.0)
    {
        discard;
    }

    fragment_color = vec4(final_color, 1.0);
}
