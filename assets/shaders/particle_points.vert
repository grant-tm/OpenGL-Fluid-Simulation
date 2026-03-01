#version 430 core

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_density;
layout(location = 2) in vec4 a_velocity;
layout(location = 3) in uint a_spatial_key;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform float u_point_size;
uniform int u_visualization_mode;
uniform float u_density_minimum;
uniform float u_density_maximum;
uniform float u_speed_minimum;
uniform float u_speed_maximum;

out vec4 v_color;

vec3 SpatialKeyColor (uint spatial_key)
{
    float key_value = float(spatial_key);
    return vec3(
        0.35 + 0.35 * sin(key_value * 0.37 + 0.0),
        0.45 + 0.35 * sin(key_value * 0.53 + 2.1),
        0.55 + 0.35 * sin(key_value * 0.71 + 4.2));
}

void main(void)
{
    gl_Position = u_projection * u_view * vec4(a_position.xyz, 1.0);
    gl_PointSize = u_point_size;

    if (u_visualization_mode == 1)
    {
        float density_range = max(u_density_maximum - u_density_minimum, 0.0001);
        float density_value = clamp((a_density.x - u_density_minimum) / density_range, 0.0, 1.0);
        v_color = vec4(density_value, density_value * density_value, 1.0 - density_value, 1.0);
    }
    else if (u_visualization_mode == 2)
    {
        float speed_range = max(u_speed_maximum - u_speed_minimum, 0.0001);
        float speed_value = clamp((length(a_velocity.xyz) - u_speed_minimum) / speed_range, 0.0, 1.0);
        v_color = vec4(0.15 + speed_value * 0.85, 0.85 - speed_value * 0.55, 1.0 - speed_value * 0.8, 1.0);
    }
    else if (u_visualization_mode == 3)
    {
        v_color = vec4(SpatialKeyColor(a_spatial_key), 1.0);
    }
    else
    {
        float density_range = max(u_density_maximum - u_density_minimum, 0.0001);
        float density_value = clamp((a_density.x - u_density_minimum) / density_range, 0.0, 1.0);
        float speed_range = max(u_speed_maximum - u_speed_minimum, 0.0001);
        float speed_value = clamp((length(a_velocity.xyz) - u_speed_minimum) / speed_range, 0.0, 1.0);

        vec3 cool_color = vec3(0.20, 0.55, 0.95);
        vec3 warm_color = vec3(0.96, 0.76, 0.30);
        vec3 accent_color = vec3(0.98, 0.36, 0.12);
        vec3 particle_color = mix(cool_color, warm_color, density_value);
        particle_color = mix(particle_color, accent_color, speed_value * 0.35);
        v_color = vec4(particle_color, 0.94);
    }
}
