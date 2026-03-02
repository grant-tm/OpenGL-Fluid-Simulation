#version 430 core

in float v_lifetime;
in float v_speed;

out vec4 fragment_color;

void main(void)
{
    vec2 centered_coordinate = gl_PointCoord * 2.0 - vec2(1.0);
    float distance_squared = dot(centered_coordinate, centered_coordinate);
    if (distance_squared > 1.0)
    {
        discard;
    }

    float alpha = (1.0 - distance_squared) * clamp(v_lifetime / 3.0, 0.30, 1.0);
    float spray_factor = clamp(v_speed / 6.0, 0.0, 1.0);
    vec3 color = mix(vec3(0.90, 0.95, 1.00), vec3(1.00, 1.00, 1.00), spray_factor);
    fragment_color = vec4(color, alpha);
}
