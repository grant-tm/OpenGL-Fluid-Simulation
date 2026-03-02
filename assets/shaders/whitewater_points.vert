#version 430 core

layout(location = 0) in vec4 a_position_and_scale;
layout(location = 1) in vec4 a_velocity_and_lifetime;

uniform mat4 u_projection;
uniform mat4 u_view;

out float v_linear_depth;

void main(void)
{
    vec3 position = a_position_and_scale.xyz;
    float scale = a_position_and_scale.w;
    float lifetime = a_velocity_and_lifetime.w;
    float speed = length(a_velocity_and_lifetime.xyz);

    if (lifetime <= 0.0 || scale <= 0.55)
    {
        gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
        gl_PointSize = 0.0;
        v_linear_depth = 10000.0;
        return;
    }

    vec3 view_position = (u_view * vec4(position, 1.0)).xyz;
    gl_Position = u_projection * vec4(view_position, 1.0);
    gl_PointSize = max(5.0, scale * 28.0);
    v_linear_depth = length(view_position);
}
