#version 430 core

out vec2 v_texture_coordinate;

void main(void)
{
    vec2 position;

    if (gl_VertexID == 0)
    {
        position = vec2(-1.0, -1.0);
    }
    else if (gl_VertexID == 1)
    {
        position = vec2(3.0, -1.0);
    }
    else
    {
        position = vec2(-1.0, 3.0);
    }

    v_texture_coordinate = position * 0.5 + vec2(0.5);
    gl_Position = vec4(position, 0.0, 1.0);
}
