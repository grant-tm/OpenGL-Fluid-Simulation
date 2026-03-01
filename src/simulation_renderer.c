#include <math.h>
#include <string.h>

#include <windows.h>

#include "opengl_helpers.h"
#include "simulation_renderer.h"

static Mat4 SimulationRenderer_CreateViewMatrix (SimulationCamera camera)
{
    f32 cosine_yaw = cosf(camera.yaw_radians);
    f32 sine_yaw = sinf(camera.yaw_radians);
    f32 cosine_pitch = cosf(camera.pitch_radians);
    f32 sine_pitch = sinf(camera.pitch_radians);

    Vec3 camera_offset = Vec3_Create(
        cosine_pitch * sine_yaw * camera.distance,
        sine_pitch * camera.distance,
        cosine_pitch * cosine_yaw * camera.distance);

    Vec3 camera_position = Vec3_Add(camera.target, camera_offset);
    Vec3 forward = Vec3_Normalize(Vec3_Subtract(camera.target, camera_position));
    Vec3 world_up = Vec3_Create(0.0f, 1.0f, 0.0f);
    Vec3 right = Vec3_Normalize(Vec3_Cross(forward, world_up));
    Vec3 up = Vec3_Cross(right, forward);

    Mat4 view_matrix = Mat4_Identity();
    view_matrix.elements[0] = right.x;
    view_matrix.elements[4] = right.y;
    view_matrix.elements[8] = right.z;

    view_matrix.elements[1] = up.x;
    view_matrix.elements[5] = up.y;
    view_matrix.elements[9] = up.z;

    view_matrix.elements[2] = -forward.x;
    view_matrix.elements[6] = -forward.y;
    view_matrix.elements[10] = -forward.z;

    view_matrix.elements[12] = -Vec3_Dot(right, camera_position);
    view_matrix.elements[13] = -Vec3_Dot(up, camera_position);
    view_matrix.elements[14] = Vec3_Dot(forward, camera_position);

    return view_matrix;
}

static bool SimulationRenderer_CreateBoundsGeometry (SimulationRenderer *renderer)
{
    static const f32 unit_cube_line_vertices[] =
    {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,
    };

    glGenVertexArrays(1, &renderer->bounds_vao_identifier);
    glGenBuffers(1, &renderer->bounds_vbo_identifier);

    if (renderer->bounds_vao_identifier == 0 || renderer->bounds_vbo_identifier == 0)
    {
        Base_LogError("Failed to create bounds geometry objects.");
        return false;
    }

    glBindVertexArray(renderer->bounds_vao_identifier);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->bounds_vbo_identifier);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unit_cube_line_vertices), unit_cube_line_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

bool SimulationRenderer_Initialize (SimulationRenderer *renderer, const SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(renderer != NULL);
    Base_Assert(particle_buffers != NULL);
    memset(renderer, 0, sizeof(*renderer));

    u32 particle_vertex_shader = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/particle_points.vert");
    u32 particle_fragment_shader = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/particle_points.frag");
    u32 line_vertex_shader = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/bounds_lines.vert");
    u32 line_fragment_shader = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/bounds_lines.frag");

    if (particle_vertex_shader == 0 || particle_fragment_shader == 0 || line_vertex_shader == 0 || line_fragment_shader == 0)
    {
        glDeleteShader(particle_vertex_shader);
        glDeleteShader(particle_fragment_shader);
        glDeleteShader(line_vertex_shader);
        glDeleteShader(line_fragment_shader);
        return false;
    }

    u32 particle_shader_identifiers[] = { particle_vertex_shader, particle_fragment_shader };
    renderer->particle_program_identifier = OpenGL_LinkProgram(
        particle_shader_identifiers,
        BASE_ARRAY_COUNT(particle_shader_identifiers),
        "particle_points");

    u32 line_shader_identifiers[] = { line_vertex_shader, line_fragment_shader };
    renderer->line_program_identifier = OpenGL_LinkProgram(
        line_shader_identifiers,
        BASE_ARRAY_COUNT(line_shader_identifiers),
        "bounds_lines");

    glDeleteShader(particle_vertex_shader);
    glDeleteShader(particle_fragment_shader);
    glDeleteShader(line_vertex_shader);
    glDeleteShader(line_fragment_shader);

    if (renderer->particle_program_identifier == 0 || renderer->line_program_identifier == 0)
    {
        SimulationRenderer_Shutdown(renderer);
        return false;
    }

    renderer->particle_projection_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_projection");
    renderer->particle_view_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_view");
    renderer->particle_color_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_color");
    renderer->particle_size_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_point_size");
    renderer->particle_visualization_mode_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_visualization_mode");
    renderer->particle_density_minimum_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_density_minimum");
    renderer->particle_density_maximum_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_density_maximum");
    renderer->particle_speed_minimum_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_speed_minimum");
    renderer->particle_speed_maximum_uniform = glGetUniformLocation(renderer->particle_program_identifier, "u_speed_maximum");

    renderer->line_projection_uniform = glGetUniformLocation(renderer->line_program_identifier, "u_projection");
    renderer->line_view_uniform = glGetUniformLocation(renderer->line_program_identifier, "u_view");
    renderer->line_color_uniform = glGetUniformLocation(renderer->line_program_identifier, "u_color");
    renderer->line_model_uniform = glGetUniformLocation(renderer->line_program_identifier, "u_model");

    glGenVertexArrays(1, &renderer->particle_vao_identifier);
    if (renderer->particle_vao_identifier == 0)
    {
        Base_LogError("Failed to create particle vertex array.");
        SimulationRenderer_Shutdown(renderer);
        return false;
    }

    glBindVertexArray(renderer->particle_vao_identifier);
    glBindBuffer(GL_ARRAY_BUFFER, particle_buffers->predicted_position_buffer.identifier);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, particle_buffers->density_buffer.identifier);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, particle_buffers->velocity_buffer.identifier);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, particle_buffers->spatial_key_buffer.identifier);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(u32), (void *) 0);
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!SimulationRenderer_CreateBoundsGeometry(renderer))
    {
        SimulationRenderer_Shutdown(renderer);
        return false;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void SimulationRenderer_Shutdown (SimulationRenderer *renderer)
{
    if (renderer == NULL) return;

    if (renderer->particle_program_identifier != 0) glDeleteProgram(renderer->particle_program_identifier);
    if (renderer->line_program_identifier != 0) glDeleteProgram(renderer->line_program_identifier);
    if (renderer->particle_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->particle_vao_identifier);
    if (renderer->bounds_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->bounds_vao_identifier);
    if (renderer->bounds_vbo_identifier != 0) glDeleteBuffers(1, &renderer->bounds_vbo_identifier);

    memset(renderer, 0, sizeof(*renderer));
}

void SimulationRenderer_UpdateCamera (SimulationCamera *camera, f32 delta_time_seconds)
{
    Base_Assert(camera != NULL);

    f32 rotation_speed = 1.8f;
    f32 zoom_speed = 8.0f;

    if ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) camera->yaw_radians -= rotation_speed * delta_time_seconds;
    if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) camera->yaw_radians += rotation_speed * delta_time_seconds;
    if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) camera->pitch_radians += rotation_speed * delta_time_seconds;
    if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) camera->pitch_radians -= rotation_speed * delta_time_seconds;
    if ((GetAsyncKeyState('W') & 0x8000) != 0) camera->distance -= zoom_speed * delta_time_seconds;
    if ((GetAsyncKeyState('S') & 0x8000) != 0) camera->distance += zoom_speed * delta_time_seconds;

    camera->pitch_radians = Base_ClampF32(camera->pitch_radians, -1.2f, 1.2f);
    camera->distance = Base_ClampF32(camera->distance, 2.0f, 30.0f);
}

void SimulationRenderer_Render (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    SimulationCamera camera,
    Vec3 simulation_bounds_size,
    SimulationParticleVisualizationMode particle_visualization_mode,
    f32 density_minimum,
    f32 density_maximum,
    f32 speed_minimum,
    f32 speed_maximum,
    i32 viewport_width,
    i32 viewport_height)
{
    Base_Assert(renderer != NULL);
    Base_Assert(particle_buffers != NULL);

    f32 aspect_ratio = (viewport_height > 0) ? ((f32) viewport_width / (f32) viewport_height) : 1.0f;
    Mat4 projection_matrix = Mat4_Perspective(60.0f * BASE_PI32 / 180.0f, aspect_ratio, 0.1f, 100.0f);
    Mat4 view_matrix = SimulationRenderer_CreateViewMatrix(camera);
    Mat4 bounds_model_matrix = Mat4_Scale(simulation_bounds_size.x, simulation_bounds_size.y, simulation_bounds_size.z);

    glViewport(0, 0, viewport_width, viewport_height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(renderer->particle_program_identifier);
    glUniformMatrix4fv(renderer->particle_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->particle_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glUniform4f(renderer->particle_color_uniform, 1.0f, 0.45f, 0.05f, 1.0f);
    glUniform1f(renderer->particle_size_uniform, 14.0f);
    glUniform1i(renderer->particle_visualization_mode_uniform, (i32) particle_visualization_mode);
    glUniform1f(renderer->particle_density_minimum_uniform, density_minimum);
    glUniform1f(renderer->particle_density_maximum_uniform, density_maximum);
    glUniform1f(renderer->particle_speed_minimum_uniform, speed_minimum);
    glUniform1f(renderer->particle_speed_maximum_uniform, speed_maximum);
    glBindVertexArray(renderer->particle_vao_identifier);
    glDrawArrays(GL_POINTS, 0, (GLsizei) particle_buffers->particle_count);

    glDisable(GL_DEPTH_TEST);
    glUseProgram(renderer->line_program_identifier);
    glUniformMatrix4fv(renderer->line_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->line_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glUniformMatrix4fv(renderer->line_model_uniform, 1, GL_FALSE, bounds_model_matrix.elements);
    glUniform4f(renderer->line_color_uniform, 0.85f, 0.88f, 0.95f, 1.0f);
    glBindVertexArray(renderer->bounds_vao_identifier);
    glDrawArrays(GL_LINES, 0, 24);

    glBindVertexArray(0);
    glUseProgram(0);
}
