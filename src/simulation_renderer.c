#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "opengl_helpers.h"
#include "simulation_renderer.h"

typedef struct SimulationCameraMatrices
{
    Vec3 position;
    Mat4 view_matrix;
    Mat4 inverse_view_matrix;
} SimulationCameraMatrices;

typedef struct SimulationLightMatrices
{
    Vec3 direction;
    Mat4 view_matrix;
    Mat4 projection_matrix;
    Mat4 view_projection_matrix;
} SimulationLightMatrices;

static void SimulationRenderer_DrawWhitewaterToFoamTexture (
    SimulationRenderer *renderer,
    const SimulationWhitewater *whitewater,
    Mat4 projection_matrix,
    Mat4 view_matrix);

static Mat4 SimulationRenderer_CreateInversePerspective (Mat4 projection_matrix)
{
    Mat4 inverse_projection_matrix = {0};
    f32 projection_x = projection_matrix.elements[0];
    f32 projection_y = projection_matrix.elements[5];
    f32 projection_z = projection_matrix.elements[10];
    f32 projection_w = projection_matrix.elements[14];

    Base_Assert(fabsf(projection_x) > BASE_EPSILON32);
    Base_Assert(fabsf(projection_y) > BASE_EPSILON32);
    Base_Assert(fabsf(projection_w) > BASE_EPSILON32);

    inverse_projection_matrix.elements[0] = 1.0f / projection_x;
    inverse_projection_matrix.elements[5] = 1.0f / projection_y;
    inverse_projection_matrix.elements[11] = 1.0f / projection_w;
    inverse_projection_matrix.elements[14] = -1.0f;
    inverse_projection_matrix.elements[15] = projection_z / projection_w;
    return inverse_projection_matrix;
}

static SimulationLightMatrices SimulationRenderer_CreateLightMatrices (Vec3 simulation_bounds_size)
{
    SimulationLightMatrices result = {0};
    Vec3 light_direction = Vec3_Normalize(Vec3_Create(-0.45f, 0.72f, 0.52f));
    Vec3 world_up = Vec3_Create(0.0f, 1.0f, 0.0f);
    Vec3 light_position = Vec3_Scale(light_direction, -20.0f);
    Vec3 forward = Vec3_Normalize(Vec3_Subtract(Vec3_Create(0.0f, 0.0f, 0.0f), light_position));
    Vec3 right = Vec3_Normalize(Vec3_Cross(forward, world_up));
    Vec3 up = Vec3_Cross(right, forward);
    f32 maximum_axis = simulation_bounds_size.x;
    f32 half_extent;

    if (simulation_bounds_size.y > maximum_axis) maximum_axis = simulation_bounds_size.y;
    if (simulation_bounds_size.z > maximum_axis) maximum_axis = simulation_bounds_size.z;
    half_extent = maximum_axis * 0.75f;

    result.direction = light_direction;
    result.view_matrix = Mat4_Identity();
    result.view_matrix.elements[0] = right.x;
    result.view_matrix.elements[4] = right.y;
    result.view_matrix.elements[8] = right.z;
    result.view_matrix.elements[1] = up.x;
    result.view_matrix.elements[5] = up.y;
    result.view_matrix.elements[9] = up.z;
    result.view_matrix.elements[2] = -forward.x;
    result.view_matrix.elements[6] = -forward.y;
    result.view_matrix.elements[10] = -forward.z;
    result.view_matrix.elements[12] = -Vec3_Dot(right, light_position);
    result.view_matrix.elements[13] = -Vec3_Dot(up, light_position);
    result.view_matrix.elements[14] = Vec3_Dot(forward, light_position);
    result.projection_matrix = Mat4_Ortho(-half_extent, half_extent, -half_extent, half_extent, 0.1f, 64.0f);
    result.view_projection_matrix = Mat4_Multiply(result.projection_matrix, result.view_matrix);
    return result;
}

static f64 SimulationRenderer_GetMillisecondsBetweenCounters (
    LARGE_INTEGER start_counter,
    LARGE_INTEGER end_counter,
    LARGE_INTEGER frequency)
{
    return
        ((f64) (end_counter.QuadPart - start_counter.QuadPart) * 1000.0) /
        (f64) frequency.QuadPart;
}

static void SimulationRenderer_BeginGpuTimerQuery (u32 query_identifier)
{
    if (query_identifier == 0) return;
    glBeginQuery(GL_TIME_ELAPSED, query_identifier);
}

static void SimulationRenderer_EndGpuTimerQuery (u32 query_identifier)
{
    if (query_identifier == 0) return;
    glEndQuery(GL_TIME_ELAPSED);
}

static void SimulationRenderer_ReadGpuTimerQuery (u32 query_identifier, f64 *milliseconds_value)
{
    GLuint query_result_available = GL_FALSE;
    GLuint64 elapsed_nanoseconds = 0;

    Base_Assert(milliseconds_value != NULL);

    if (query_identifier == 0) return;

    glGetQueryObjectuiv(query_identifier, GL_QUERY_RESULT_AVAILABLE, &query_result_available);
    if (query_result_available == GL_FALSE) return;

    glGetQueryObjectui64v(query_identifier, GL_QUERY_RESULT, &elapsed_nanoseconds);
    *milliseconds_value = (f64) elapsed_nanoseconds / 1000000.0;
}

static void SimulationRenderer_ReadGpuTimestampRange (
    u32 start_query_identifier,
    u32 end_query_identifier,
    f64 *milliseconds_value)
{
    GLuint start_query_result_available = GL_FALSE;
    GLuint end_query_result_available = GL_FALSE;
    GLuint64 start_timestamp_nanoseconds = 0;
    GLuint64 end_timestamp_nanoseconds = 0;

    Base_Assert(milliseconds_value != NULL);

    if (start_query_identifier == 0 || end_query_identifier == 0) return;

    glGetQueryObjectuiv(start_query_identifier, GL_QUERY_RESULT_AVAILABLE, &start_query_result_available);
    glGetQueryObjectuiv(end_query_identifier, GL_QUERY_RESULT_AVAILABLE, &end_query_result_available);
    if (start_query_result_available == GL_FALSE || end_query_result_available == GL_FALSE) return;

    glGetQueryObjectui64v(start_query_identifier, GL_QUERY_RESULT, &start_timestamp_nanoseconds);
    glGetQueryObjectui64v(end_query_identifier, GL_QUERY_RESULT, &end_timestamp_nanoseconds);
    if (end_timestamp_nanoseconds < start_timestamp_nanoseconds) return;

    *milliseconds_value = (f64) (end_timestamp_nanoseconds - start_timestamp_nanoseconds) / 1000000.0;
}

static SimulationCameraMatrices SimulationRenderer_CreateCameraMatrices (SimulationCamera camera)
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

    SimulationCameraMatrices result = {0};
    result.position = camera_position;
    result.view_matrix = Mat4_Identity();
    result.view_matrix.elements[0] = right.x;
    result.view_matrix.elements[4] = right.y;
    result.view_matrix.elements[8] = right.z;
    result.view_matrix.elements[1] = up.x;
    result.view_matrix.elements[5] = up.y;
    result.view_matrix.elements[9] = up.z;
    result.view_matrix.elements[2] = -forward.x;
    result.view_matrix.elements[6] = -forward.y;
    result.view_matrix.elements[10] = -forward.z;
    result.view_matrix.elements[12] = -Vec3_Dot(right, camera_position);
    result.view_matrix.elements[13] = -Vec3_Dot(up, camera_position);
    result.view_matrix.elements[14] = Vec3_Dot(forward, camera_position);
    result.inverse_view_matrix = Mat4_Identity();
    result.inverse_view_matrix.elements[0] = right.x;
    result.inverse_view_matrix.elements[1] = right.y;
    result.inverse_view_matrix.elements[2] = right.z;
    result.inverse_view_matrix.elements[4] = up.x;
    result.inverse_view_matrix.elements[5] = up.y;
    result.inverse_view_matrix.elements[6] = up.z;
    result.inverse_view_matrix.elements[8] = -forward.x;
    result.inverse_view_matrix.elements[9] = -forward.y;
    result.inverse_view_matrix.elements[10] = -forward.z;
    result.inverse_view_matrix.elements[12] = camera_position.x;
    result.inverse_view_matrix.elements[13] = camera_position.y;
    result.inverse_view_matrix.elements[14] = camera_position.z;
    return result;
}

static bool SimulationRenderer_CreateBoundsGeometry (SimulationRenderer *renderer)
{
    static const f32 line_vertices[] =
    {
        -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f,
        0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f, -0.5f,-0.5f,-0.5f,
        -0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f, 0.5f,0.5f,0.5f,
        0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f, -0.5f,-0.5f,0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,0.5f,
        0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f, -0.5f,0.5f,-0.5f, -0.5f,0.5f,0.5f,
    };
    glGenVertexArrays(1, &renderer->bounds_vao_identifier);
    glGenBuffers(1, &renderer->bounds_vbo_identifier);
    if (renderer->bounds_vao_identifier == 0 || renderer->bounds_vbo_identifier == 0) return false;
    glBindVertexArray(renderer->bounds_vao_identifier);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->bounds_vbo_identifier);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

static bool SimulationRenderer_CreateScreenFluidQuadGeometry (SimulationRenderer *renderer, const SimulationParticleBuffers *particle_buffers)
{
    static const f32 quad_vertices[] =
    {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &renderer->screen_fluid_quad_vao_identifier);
    glGenBuffers(1, &renderer->screen_fluid_quad_vbo_identifier);
    if (renderer->screen_fluid_quad_vao_identifier == 0 || renderer->screen_fluid_quad_vbo_identifier == 0) return false;

    glBindVertexArray(renderer->screen_fluid_quad_vao_identifier);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->screen_fluid_quad_vbo_identifier);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) (2 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, particle_buffers->predicted_position_buffer.identifier);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

static void SimulationRenderer_BindScreenFluidQuadGeometry (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers)
{
    Base_Assert(renderer != NULL);
    Base_Assert(particle_buffers != NULL);

    glBindVertexArray(renderer->screen_fluid_quad_vao_identifier);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->screen_fluid_quad_vbo_identifier);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) (2 * sizeof(f32)));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);

    glBindBuffer(GL_ARRAY_BUFFER, particle_buffers->predicted_position_buffer.identifier);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static bool SimulationRenderer_CreateFullscreenGeometry (SimulationRenderer *renderer)
{
    glGenVertexArrays(1, &renderer->fullscreen_vao_identifier);
    return renderer->fullscreen_vao_identifier != 0;
}

static bool SimulationRenderer_CreateProgramSet (SimulationRenderer *renderer)
{
    u32 particle_vs = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/particle_points.vert");
    u32 particle_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/particle_points.frag");
    u32 line_vs = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/bounds_lines.vert");
    u32 line_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/bounds_lines.frag");
    u32 fluid_thickness_vs = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/screen_fluid_thickness.vert");
    u32 fluid_thickness_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_thickness.frag");
    u32 fluid_depth_vs = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/screen_fluid_depth.vert");
    u32 fluid_depth_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_depth.frag");
    u32 fullscreen_vs = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/fullscreen_quad.vert");
    u32 prepare_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_prepare.frag");
    u32 blur_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_blur.frag");
    u32 normal_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_normal.frag");
    u32 debug_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_debug.frag");
    u32 shadow_blur_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_shadow_blur.frag");
    u32 composite_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/screen_fluid_composite.frag");
    u32 whitewater_vs = OpenGL_CompileShaderFromFile(GL_VERTEX_SHADER, "assets/shaders/whitewater_points.vert");
    u32 whitewater_fs = OpenGL_CompileShaderFromFile(GL_FRAGMENT_SHADER, "assets/shaders/whitewater_points.frag");
    if (particle_vs == 0 || particle_fs == 0 || line_vs == 0 || line_fs == 0 ||
        fluid_thickness_vs == 0 || fluid_thickness_fs == 0 || fluid_depth_vs == 0 || fluid_depth_fs == 0 ||
        fullscreen_vs == 0 || prepare_fs == 0 || blur_fs == 0 || normal_fs == 0 || debug_fs == 0 ||
        shadow_blur_fs == 0 || composite_fs == 0 || whitewater_vs == 0 || whitewater_fs == 0)
    {
        glDeleteShader(particle_vs); glDeleteShader(particle_fs); glDeleteShader(line_vs); glDeleteShader(line_fs);
        glDeleteShader(fluid_thickness_vs); glDeleteShader(fluid_thickness_fs);
        glDeleteShader(fluid_depth_vs); glDeleteShader(fluid_depth_fs);
        glDeleteShader(fullscreen_vs); glDeleteShader(prepare_fs); glDeleteShader(blur_fs); glDeleteShader(normal_fs);
        glDeleteShader(debug_fs); glDeleteShader(shadow_blur_fs); glDeleteShader(composite_fs);
        glDeleteShader(whitewater_vs); glDeleteShader(whitewater_fs);
        return false;
    }
    u32 pair[2];
    pair[0] = particle_vs; pair[1] = particle_fs; renderer->particle_program_identifier = OpenGL_LinkProgram(pair, 2, "particle_points");
    pair[0] = line_vs; pair[1] = line_fs; renderer->line_program_identifier = OpenGL_LinkProgram(pair, 2, "bounds_lines");
    pair[0] = fluid_thickness_vs; pair[1] = fluid_thickness_fs; renderer->screen_fluid_thickness_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_thickness");
    pair[0] = fluid_depth_vs; pair[1] = fluid_depth_fs; renderer->screen_fluid_depth_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_depth");
    pair[0] = fullscreen_vs; pair[1] = prepare_fs; renderer->screen_fluid_prepare_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_prepare");
    pair[0] = fullscreen_vs; pair[1] = blur_fs; renderer->screen_fluid_blur_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_blur");
    pair[0] = fullscreen_vs; pair[1] = normal_fs; renderer->screen_fluid_normal_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_normal");
    pair[0] = fullscreen_vs; pair[1] = debug_fs; renderer->screen_fluid_debug_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_debug");
    pair[0] = fullscreen_vs; pair[1] = shadow_blur_fs; renderer->screen_fluid_shadow_blur_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_shadow_blur");
    pair[0] = fullscreen_vs; pair[1] = composite_fs; renderer->screen_fluid_composite_program_identifier = OpenGL_LinkProgram(pair, 2, "screen_fluid_composite");
    pair[0] = whitewater_vs; pair[1] = whitewater_fs; renderer->whitewater_program_identifier = OpenGL_LinkProgram(pair, 2, "whitewater_points");
    glDeleteShader(particle_vs); glDeleteShader(particle_fs); glDeleteShader(line_vs); glDeleteShader(line_fs);
    glDeleteShader(fluid_thickness_vs); glDeleteShader(fluid_thickness_fs);
    glDeleteShader(fluid_depth_vs); glDeleteShader(fluid_depth_fs);
    glDeleteShader(fullscreen_vs); glDeleteShader(prepare_fs); glDeleteShader(blur_fs); glDeleteShader(normal_fs);
    glDeleteShader(debug_fs); glDeleteShader(shadow_blur_fs); glDeleteShader(composite_fs);
    glDeleteShader(whitewater_vs); glDeleteShader(whitewater_fs);
    return renderer->particle_program_identifier != 0 &&
        renderer->line_program_identifier != 0 &&
        renderer->screen_fluid_thickness_program_identifier != 0 &&
        renderer->screen_fluid_depth_program_identifier != 0 &&
        renderer->screen_fluid_prepare_program_identifier != 0 &&
        renderer->screen_fluid_blur_program_identifier != 0 &&
        renderer->screen_fluid_normal_program_identifier != 0 &&
        renderer->screen_fluid_debug_program_identifier != 0 &&
        renderer->screen_fluid_shadow_blur_program_identifier != 0 &&
        renderer->screen_fluid_composite_program_identifier != 0 &&
        renderer->whitewater_program_identifier != 0;
}

static void SimulationRenderer_QueryUniforms (SimulationRenderer *renderer)
{
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
    renderer->whitewater_projection_uniform = glGetUniformLocation(renderer->whitewater_program_identifier, "u_projection");
    renderer->whitewater_view_uniform = glGetUniformLocation(renderer->whitewater_program_identifier, "u_view");
    renderer->screen_fluid_thickness_projection_uniform = glGetUniformLocation(renderer->screen_fluid_thickness_program_identifier, "u_projection");
    renderer->screen_fluid_thickness_view_uniform = glGetUniformLocation(renderer->screen_fluid_thickness_program_identifier, "u_view");
    renderer->screen_fluid_thickness_point_size_uniform = glGetUniformLocation(renderer->screen_fluid_thickness_program_identifier, "u_point_size");
    renderer->screen_fluid_depth_projection_uniform = glGetUniformLocation(renderer->screen_fluid_depth_program_identifier, "u_projection");
    renderer->screen_fluid_depth_view_uniform = glGetUniformLocation(renderer->screen_fluid_depth_program_identifier, "u_view");
    renderer->screen_fluid_depth_point_size_uniform = glGetUniformLocation(renderer->screen_fluid_depth_program_identifier, "u_point_size");
    renderer->screen_fluid_depth_viewport_height_uniform = glGetUniformLocation(renderer->screen_fluid_depth_program_identifier, "u_viewport_height");
    renderer->screen_fluid_prepare_depth_texture_uniform = glGetUniformLocation(renderer->screen_fluid_prepare_program_identifier, "u_depth_texture");
    renderer->screen_fluid_prepare_thickness_texture_uniform = glGetUniformLocation(renderer->screen_fluid_prepare_program_identifier, "u_thickness_texture");
    renderer->screen_fluid_blur_texture_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_source_texture");
    renderer->screen_fluid_blur_filter_mode_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_filter_mode");
    renderer->screen_fluid_blur_depth_texture_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_depth_texture");
    renderer->screen_fluid_blur_direction_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_blur_direction");
    renderer->screen_fluid_blur_texel_size_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_texel_size");
    renderer->screen_fluid_blur_projection_scale_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_projection_scale_x");
    renderer->screen_fluid_blur_image_width_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_image_width");
    renderer->screen_fluid_blur_world_radius_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_world_radius");
    renderer->screen_fluid_blur_max_radius_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_max_screen_radius");
    renderer->screen_fluid_blur_strength_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_strength");
    renderer->screen_fluid_blur_difference_strength_uniform = glGetUniformLocation(renderer->screen_fluid_blur_program_identifier, "u_difference_strength");
    renderer->screen_fluid_normal_comp_texture_uniform = glGetUniformLocation(renderer->screen_fluid_normal_program_identifier, "u_comp_texture");
    renderer->screen_fluid_normal_texel_size_uniform = glGetUniformLocation(renderer->screen_fluid_normal_program_identifier, "u_texel_size");
    renderer->screen_fluid_normal_projection_uniform = glGetUniformLocation(renderer->screen_fluid_normal_program_identifier, "u_projection");
    renderer->screen_fluid_normal_inverse_projection_uniform = glGetUniformLocation(renderer->screen_fluid_normal_program_identifier, "u_inverse_projection");
    renderer->screen_fluid_debug_texture_uniform = glGetUniformLocation(renderer->screen_fluid_debug_program_identifier, "u_debug_texture");
    renderer->screen_fluid_debug_mode_uniform = glGetUniformLocation(renderer->screen_fluid_debug_program_identifier, "u_debug_mode");
    renderer->screen_fluid_shadow_blur_texture_uniform = glGetUniformLocation(renderer->screen_fluid_shadow_blur_program_identifier, "u_source_texture");
    renderer->screen_fluid_shadow_blur_direction_uniform = glGetUniformLocation(renderer->screen_fluid_shadow_blur_program_identifier, "u_blur_direction");
    renderer->screen_fluid_shadow_blur_texel_size_uniform = glGetUniformLocation(renderer->screen_fluid_shadow_blur_program_identifier, "u_texel_size");
    renderer->screen_fluid_composite_texture_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_fluid_texture");
    renderer->screen_fluid_composite_depth_texture_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_depth_texture");
    renderer->screen_fluid_composite_texel_size_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_texel_size");
    renderer->screen_fluid_composite_projection_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_projection");
    renderer->screen_fluid_composite_inverse_projection_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_inverse_projection");
    renderer->screen_fluid_composite_inverse_view_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_inverse_view");
    renderer->screen_fluid_composite_bounds_size_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_bounds_size");
    renderer->screen_fluid_composite_normal_texture_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_normal_texture");
    renderer->screen_fluid_composite_scene_texture_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_scene_texture");
    renderer->screen_fluid_composite_foam_texture_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_foam_texture");
    renderer->screen_fluid_composite_shadow_texture_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_shadow_texture");
    renderer->screen_fluid_composite_shadow_view_projection_uniform = glGetUniformLocation(renderer->screen_fluid_composite_program_identifier, "u_shadow_view_projection");
}

static bool SimulationRenderer_CreateParticleGeometry (SimulationRenderer *renderer, const SimulationParticleBuffers *particle_buffers)
{
    glGenVertexArrays(1, &renderer->particle_vao_identifier);
    if (renderer->particle_vao_identifier == 0) return false;
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
    return true;
}

static bool SimulationRenderer_CreateWhitewaterGeometry (
    SimulationRenderer *renderer,
    const SimulationWhitewater *whitewater)
{
    Base_Assert(renderer != NULL);
    Base_Assert(whitewater != NULL);

    glGenVertexArrays(1, &renderer->whitewater_vao_identifier);
    if (renderer->whitewater_vao_identifier == 0) return false;

    glBindVertexArray(renderer->whitewater_vao_identifier);
    glBindBuffer(GL_ARRAY_BUFFER, whitewater->particle_buffer.identifier);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (4 * sizeof(f32)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

static bool SimulationRenderer_AllocateScreenFluidTargets (
    SimulationRenderer *renderer,
    i32 texture_width,
    i32 texture_height,
    i32 thickness_texture_width,
    i32 thickness_texture_height)
{
    renderer->screen_fluid_thickness_texture_width = thickness_texture_width;
    renderer->screen_fluid_thickness_texture_height = thickness_texture_height;
    renderer->screen_fluid_texture_width = texture_width;
    renderer->screen_fluid_texture_height = texture_height;
    renderer->screen_fluid_shadow_texture_width = texture_width / 4;
    renderer->screen_fluid_shadow_texture_height = texture_height / 4;
    if (renderer->screen_fluid_shadow_texture_width < 128) renderer->screen_fluid_shadow_texture_width = 128;
    if (renderer->screen_fluid_shadow_texture_height < 128) renderer->screen_fluid_shadow_texture_height = 128;
    if (renderer->screen_fluid_framebuffer_identifier == 0) glGenFramebuffers(1, &renderer->screen_fluid_framebuffer_identifier);
    if (renderer->screen_fluid_blur_framebuffer_identifier == 0) glGenFramebuffers(1, &renderer->screen_fluid_blur_framebuffer_identifier);
    if (renderer->screen_fluid_thickness_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_thickness_texture_identifier);
    if (renderer->screen_fluid_depth_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_depth_texture_identifier);
    if (renderer->screen_fluid_comp_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_comp_texture_identifier);
    if (renderer->screen_fluid_comp_blur_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_comp_blur_texture_identifier);
    if (renderer->screen_fluid_normal_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_normal_texture_identifier);
    if (renderer->screen_fluid_scene_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_scene_texture_identifier);
    if (renderer->screen_fluid_foam_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_foam_texture_identifier);
    if (renderer->screen_fluid_shadow_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_shadow_texture_identifier);
    if (renderer->screen_fluid_shadow_blur_texture_identifier == 0) glGenTextures(1, &renderer->screen_fluid_shadow_blur_texture_identifier);
    if (renderer->screen_fluid_depth_renderbuffer_identifier == 0) glGenRenderbuffers(1, &renderer->screen_fluid_depth_renderbuffer_identifier);
    if (renderer->screen_fluid_framebuffer_identifier == 0 || renderer->screen_fluid_blur_framebuffer_identifier == 0 ||
        renderer->screen_fluid_thickness_texture_identifier == 0 || renderer->screen_fluid_depth_texture_identifier == 0 ||
        renderer->screen_fluid_comp_texture_identifier == 0 || renderer->screen_fluid_comp_blur_texture_identifier == 0 ||
        renderer->screen_fluid_normal_texture_identifier == 0 || renderer->screen_fluid_scene_texture_identifier == 0 ||
        renderer->screen_fluid_foam_texture_identifier == 0 ||
        renderer->screen_fluid_shadow_texture_identifier == 0 || renderer->screen_fluid_shadow_blur_texture_identifier == 0 ||
        renderer->screen_fluid_depth_renderbuffer_identifier == 0) return false;
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_thickness_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, thickness_texture_width, thickness_texture_height, 0, GL_RED, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texture_width, texture_height, 0, GL_RED, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_blur_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_normal_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_scene_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_foam_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_shadow_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        renderer->screen_fluid_shadow_texture_width,
        renderer->screen_fluid_shadow_texture_height,
        0,
        GL_RED,
        GL_FLOAT,
        NULL);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_shadow_blur_texture_identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        renderer->screen_fluid_shadow_texture_width,
        renderer->screen_fluid_shadow_texture_height,
        0,
        GL_RED,
        GL_FLOAT,
        NULL);
    glBindRenderbuffer(GL_RENDERBUFFER, renderer->screen_fluid_depth_renderbuffer_identifier);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, texture_width, texture_height);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_normal_texture_identifier, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderer->screen_fluid_depth_renderbuffer_identifier);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_blur_framebuffer_identifier);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_blur_texture_identifier, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glBindTexture(GL_TEXTURE_2D, 0); glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return true;
}

static bool SimulationRenderer_EnsureScreenFluidTargets (SimulationRenderer *renderer, i32 viewport_width, i32 viewport_height)
{
    const f32 screen_fluid_resolution_scale = 0.75f;
    i32 full_resolution_width = viewport_width;
    i32 full_resolution_height = viewport_height;
    i32 texture_width = (i32) ((f32) viewport_width * screen_fluid_resolution_scale);
    i32 texture_height = (i32) ((f32) viewport_height * screen_fluid_resolution_scale);
    f32 viewport_aspect = (f32) texture_height / (f32) texture_width;
    i32 thickness_texture_max_width = (1280 < full_resolution_width) ? 1280 : full_resolution_width;
    i32 thickness_texture_max_height = (((i32) (1280.0f * viewport_aspect)) < full_resolution_height) ?
        ((i32) (1280.0f * viewport_aspect)) :
        full_resolution_height;
    i32 thickness_texture_width = (thickness_texture_max_width > (full_resolution_width / 2)) ?
        thickness_texture_max_width :
        (full_resolution_width / 2);
    i32 thickness_texture_height = (thickness_texture_max_height > (full_resolution_height / 2)) ?
        thickness_texture_max_height :
        (full_resolution_height / 2);
    if (texture_width < 64) texture_width = 64;
    if (texture_height < 64) texture_height = 64;
    if (thickness_texture_width < 64) thickness_texture_width = 64;
    if (thickness_texture_height < 64) thickness_texture_height = 64;
    if (renderer->screen_fluid_texture_width == texture_width &&
        renderer->screen_fluid_texture_height == texture_height &&
        renderer->screen_fluid_thickness_texture_width == thickness_texture_width &&
        renderer->screen_fluid_thickness_texture_height == thickness_texture_height &&
        renderer->screen_fluid_thickness_texture_identifier != 0 &&
        renderer->screen_fluid_comp_texture_identifier != 0 &&
        renderer->screen_fluid_normal_texture_identifier != 0 &&
        renderer->screen_fluid_scene_texture_identifier != 0 &&
        renderer->screen_fluid_foam_texture_identifier != 0 &&
        renderer->screen_fluid_shadow_texture_identifier != 0 &&
        renderer->screen_fluid_shadow_blur_texture_identifier != 0) return true;
    if (renderer->screen_fluid_thickness_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_thickness_texture_identifier);
    if (renderer->screen_fluid_depth_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_depth_texture_identifier);
    if (renderer->screen_fluid_depth_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_depth_blur_texture_identifier);
    if (renderer->screen_fluid_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_blur_texture_identifier);
    if (renderer->screen_fluid_comp_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_comp_texture_identifier);
    if (renderer->screen_fluid_comp_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_comp_blur_texture_identifier);
    if (renderer->screen_fluid_normal_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_normal_texture_identifier);
    if (renderer->screen_fluid_scene_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_scene_texture_identifier);
    if (renderer->screen_fluid_foam_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_foam_texture_identifier);
    if (renderer->screen_fluid_shadow_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_shadow_texture_identifier);
    if (renderer->screen_fluid_shadow_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_shadow_blur_texture_identifier);
    if (renderer->screen_fluid_depth_renderbuffer_identifier != 0) glDeleteRenderbuffers(1, &renderer->screen_fluid_depth_renderbuffer_identifier);
    renderer->screen_fluid_thickness_texture_identifier = 0;
    renderer->screen_fluid_depth_texture_identifier = 0;
    renderer->screen_fluid_depth_blur_texture_identifier = 0;
    renderer->screen_fluid_blur_texture_identifier = 0;
    renderer->screen_fluid_comp_texture_identifier = 0;
    renderer->screen_fluid_comp_blur_texture_identifier = 0;
    renderer->screen_fluid_normal_texture_identifier = 0;
    renderer->screen_fluid_scene_texture_identifier = 0;
    renderer->screen_fluid_foam_texture_identifier = 0;
    renderer->screen_fluid_shadow_texture_identifier = 0;
    renderer->screen_fluid_shadow_blur_texture_identifier = 0;
    renderer->screen_fluid_depth_renderbuffer_identifier = 0;
    renderer->screen_fluid_shadow_texture_width = 0;
    renderer->screen_fluid_shadow_texture_height = 0;
    renderer->screen_fluid_thickness_texture_width = 0;
    renderer->screen_fluid_thickness_texture_height = 0;
    return SimulationRenderer_AllocateScreenFluidTargets(
        renderer,
        texture_width,
        texture_height,
        thickness_texture_width,
        thickness_texture_height);
}

bool SimulationRenderer_Initialize (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    const SimulationWhitewater *whitewater);
void SimulationRenderer_Shutdown (SimulationRenderer *renderer);
void SimulationRenderer_UpdateCamera (SimulationCamera *camera, f32 delta_time_seconds);

bool SimulationRenderer_Initialize (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    const SimulationWhitewater *whitewater)
{
    Base_Assert(renderer != NULL);
    Base_Assert(particle_buffers != NULL);
    Base_Assert(whitewater != NULL);
    memset(renderer, 0, sizeof(*renderer));
    if (!SimulationRenderer_CreateProgramSet(renderer)) return false;
    SimulationRenderer_QueryUniforms(renderer);
    if (!SimulationRenderer_CreateParticleGeometry(renderer, particle_buffers) ||
        !SimulationRenderer_CreateWhitewaterGeometry(renderer, whitewater) ||
        !SimulationRenderer_CreateScreenFluidQuadGeometry(renderer, particle_buffers) ||
        !SimulationRenderer_CreateBoundsGeometry(renderer) ||
        !SimulationRenderer_CreateFullscreenGeometry(renderer))
    {
        SimulationRenderer_Shutdown(renderer);
        return false;
    }
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_scene_copy_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_shadow_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_surface_inputs_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_blur_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_normal_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_composite_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_total_start_query_identifiers);
    glGenQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_total_end_query_identifiers);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void SimulationRenderer_Shutdown (SimulationRenderer *renderer)
{
    if (renderer == NULL) return;
    if (renderer->particle_program_identifier != 0) glDeleteProgram(renderer->particle_program_identifier);
    if (renderer->line_program_identifier != 0) glDeleteProgram(renderer->line_program_identifier);
    if (renderer->whitewater_program_identifier != 0) glDeleteProgram(renderer->whitewater_program_identifier);
    if (renderer->screen_fluid_thickness_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_thickness_program_identifier);
    if (renderer->screen_fluid_depth_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_depth_program_identifier);
    if (renderer->screen_fluid_prepare_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_prepare_program_identifier);
    if (renderer->screen_fluid_blur_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_blur_program_identifier);
    if (renderer->screen_fluid_normal_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_normal_program_identifier);
    if (renderer->screen_fluid_debug_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_debug_program_identifier);
    if (renderer->screen_fluid_shadow_blur_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_shadow_blur_program_identifier);
    if (renderer->screen_fluid_composite_program_identifier != 0) glDeleteProgram(renderer->screen_fluid_composite_program_identifier);
    if (renderer->particle_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->particle_vao_identifier);
    if (renderer->screen_fluid_quad_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->screen_fluid_quad_vao_identifier);
    if (renderer->screen_fluid_quad_vbo_identifier != 0) glDeleteBuffers(1, &renderer->screen_fluid_quad_vbo_identifier);
    if (renderer->bounds_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->bounds_vao_identifier);
    if (renderer->bounds_vbo_identifier != 0) glDeleteBuffers(1, &renderer->bounds_vbo_identifier);
    if (renderer->whitewater_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->whitewater_vao_identifier);
    if (renderer->fullscreen_vao_identifier != 0) glDeleteVertexArrays(1, &renderer->fullscreen_vao_identifier);
    if (renderer->screen_fluid_framebuffer_identifier != 0) glDeleteFramebuffers(1, &renderer->screen_fluid_framebuffer_identifier);
    if (renderer->screen_fluid_blur_framebuffer_identifier != 0) glDeleteFramebuffers(1, &renderer->screen_fluid_blur_framebuffer_identifier);
    if (renderer->screen_fluid_depth_renderbuffer_identifier != 0) glDeleteRenderbuffers(1, &renderer->screen_fluid_depth_renderbuffer_identifier);
    if (renderer->screen_fluid_thickness_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_thickness_texture_identifier);
    if (renderer->screen_fluid_depth_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_depth_texture_identifier);
    if (renderer->screen_fluid_depth_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_depth_blur_texture_identifier);
    if (renderer->screen_fluid_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_blur_texture_identifier);
    if (renderer->screen_fluid_comp_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_comp_texture_identifier);
    if (renderer->screen_fluid_comp_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_comp_blur_texture_identifier);
    if (renderer->screen_fluid_normal_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_normal_texture_identifier);
    if (renderer->screen_fluid_scene_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_scene_texture_identifier);
    if (renderer->screen_fluid_foam_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_foam_texture_identifier);
    if (renderer->screen_fluid_shadow_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_shadow_texture_identifier);
    if (renderer->screen_fluid_shadow_blur_texture_identifier != 0) glDeleteTextures(1, &renderer->screen_fluid_shadow_blur_texture_identifier);
    if (renderer->gpu_scene_copy_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_scene_copy_query_identifiers);
    if (renderer->gpu_shadow_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_shadow_query_identifiers);
    if (renderer->gpu_surface_inputs_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_surface_inputs_query_identifiers);
    if (renderer->gpu_blur_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_blur_query_identifiers);
    if (renderer->gpu_normal_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_normal_query_identifiers);
    if (renderer->gpu_composite_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_composite_query_identifiers);
    if (renderer->gpu_total_start_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_total_start_query_identifiers);
    if (renderer->gpu_total_end_query_identifiers[0] != 0) glDeleteQueries(SIMULATION_RENDERER_GPU_QUERY_RING_SIZE, renderer->gpu_total_end_query_identifiers);
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

static void SimulationRenderer_DrawParticles (
    SimulationRenderer *renderer, const SimulationParticleBuffers *particle_buffers, Mat4 projection_matrix, Mat4 view_matrix,
    SimulationParticleVisualizationMode particle_visualization_mode, f32 density_minimum, f32 density_maximum, f32 speed_minimum, f32 speed_maximum)
{
    glUseProgram(renderer->particle_program_identifier);
    glUniformMatrix4fv(renderer->particle_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->particle_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glUniform4f(renderer->particle_color_uniform, 1.0f, 0.45f, 0.05f, 1.0f);
    glUniform1f(renderer->particle_size_uniform, 16.0f);
    glUniform1i(renderer->particle_visualization_mode_uniform, (i32) particle_visualization_mode);
    glUniform1f(renderer->particle_density_minimum_uniform, density_minimum);
    glUniform1f(renderer->particle_density_maximum_uniform, density_maximum);
    glUniform1f(renderer->particle_speed_minimum_uniform, speed_minimum);
    glUniform1f(renderer->particle_speed_maximum_uniform, speed_maximum);
    glBindVertexArray(renderer->particle_vao_identifier);
    glDrawArrays(GL_POINTS, 0, (GLsizei) particle_buffers->particle_count);
}

static void SimulationRenderer_DrawScreenFluid (
    SimulationRenderer *renderer, const SimulationParticleBuffers *particle_buffers, const SimulationWhitewater *whitewater, Mat4 projection_matrix, Mat4 view_matrix,
    Mat4 inverse_view_matrix, Vec3 simulation_bounds_size, Vec3 camera_position,
    SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode,
    SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode,
    i32 viewport_width, i32 viewport_height)
{
    LARGE_INTEGER performance_frequency = {0};
    LARGE_INTEGER pass_start_counter = {0};
    LARGE_INTEGER pass_end_counter = {0};
    Vec2 texel_size;
    Vec2 shadow_texel_size;
    Mat4 inverse_projection_matrix;
    SimulationLightMatrices light_matrices;
    Base_Assert(whitewater != NULL);
    const f32 thickness_particle_radius = 0.175f;
    const f32 depth_particle_radius = 0.25f;
    const f32 blur_world_radius = 0.26f;
    const i32 blur_max_screen_radius = 32;
    const i32 blur_iteration_count = 5;
    const f32 blur_strength = 0.45f;
    const f32 blur_difference_strength = 3.7f;
    u32 final_comp_texture_identifier = renderer->screen_fluid_comp_texture_identifier;
    u32 gpu_query_identifier_index = renderer->gpu_query_frame_index;
    if (!SimulationRenderer_EnsureScreenFluidTargets(renderer, viewport_width, viewport_height)) return;
    QueryPerformanceFrequency(&performance_frequency);
    inverse_projection_matrix = SimulationRenderer_CreateInversePerspective(projection_matrix);
    light_matrices = SimulationRenderer_CreateLightMatrices(simulation_bounds_size);
    texel_size = Vec2_Create(1.0f / (f32) renderer->screen_fluid_texture_width, 1.0f / (f32) renderer->screen_fluid_texture_height);
    shadow_texel_size = Vec2_Create(
        1.0f / (f32) renderer->screen_fluid_shadow_texture_width,
        1.0f / (f32) renderer->screen_fluid_shadow_texture_height);

    QueryPerformanceCounter(&pass_start_counter);
    SimulationRenderer_BeginGpuTimerQuery(renderer->gpu_scene_copy_query_identifiers[gpu_query_identifier_index]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_scene_texture_identifier);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, renderer->screen_fluid_texture_width, renderer->screen_fluid_texture_height);
    glBindTexture(GL_TEXTURE_2D, 0);
    SimulationRenderer_EndGpuTimerQuery(renderer->gpu_scene_copy_query_identifiers[gpu_query_identifier_index]);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.scene_copy_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

    QueryPerformanceCounter(&pass_start_counter);
    SimulationRenderer_BeginGpuTimerQuery(renderer->gpu_shadow_query_identifiers[gpu_query_identifier_index]);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_blur_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_shadow_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, renderer->screen_fluid_shadow_texture_width, renderer->screen_fluid_shadow_texture_height);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(renderer->screen_fluid_thickness_program_identifier);
    glUniformMatrix4fv(renderer->screen_fluid_thickness_projection_uniform, 1, GL_FALSE, light_matrices.projection_matrix.elements);
    glUniformMatrix4fv(renderer->screen_fluid_thickness_view_uniform, 1, GL_FALSE, light_matrices.view_matrix.elements);
    glUniform1f(renderer->screen_fluid_thickness_point_size_uniform, thickness_particle_radius);
    SimulationRenderer_BindScreenFluidQuadGeometry(renderer, particle_buffers);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei) particle_buffers->particle_count);
    glDisable(GL_BLEND);

    glUseProgram(renderer->screen_fluid_shadow_blur_program_identifier);
    glUniform1i(renderer->screen_fluid_shadow_blur_texture_uniform, 0);
    glUniform2f(renderer->screen_fluid_shadow_blur_texel_size_uniform, shadow_texel_size.x, shadow_texel_size.y);
    glBindVertexArray(renderer->fullscreen_vao_identifier);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_shadow_blur_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2f(renderer->screen_fluid_shadow_blur_direction_uniform, 1.0f, 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_shadow_texture_identifier);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_blur_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_shadow_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2f(renderer->screen_fluid_shadow_blur_direction_uniform, 0.0f, 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_shadow_blur_texture_identifier);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    SimulationRenderer_EndGpuTimerQuery(renderer->gpu_shadow_query_identifiers[gpu_query_identifier_index]);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.shadow_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

    QueryPerformanceCounter(&pass_start_counter);
    SimulationRenderer_BeginGpuTimerQuery(renderer->gpu_surface_inputs_query_identifiers[gpu_query_identifier_index]);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_foam_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, renderer->screen_fluid_texture_width, renderer->screen_fluid_texture_height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 1.0f, 10000.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SimulationRenderer_DrawWhitewaterToFoamTexture(renderer, whitewater, projection_matrix, view_matrix);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_thickness_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, renderer->screen_fluid_thickness_texture_width, renderer->screen_fluid_thickness_texture_height);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(renderer->screen_fluid_thickness_program_identifier);
    glUniformMatrix4fv(renderer->screen_fluid_thickness_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->screen_fluid_thickness_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glUniform1f(renderer->screen_fluid_thickness_point_size_uniform, thickness_particle_radius);
    SimulationRenderer_BindScreenFluidQuadGeometry(renderer, particle_buffers);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei) particle_buffers->particle_count);
    glDisable(GL_BLEND);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, renderer->screen_fluid_texture_width, renderer->screen_fluid_texture_height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(10000.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(renderer->screen_fluid_depth_program_identifier);
    glUniformMatrix4fv(renderer->screen_fluid_depth_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->screen_fluid_depth_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glUniform1f(renderer->screen_fluid_depth_point_size_uniform, depth_particle_radius);
    glUniform1f(renderer->screen_fluid_depth_viewport_height_uniform, (f32) renderer->screen_fluid_texture_height);
    SimulationRenderer_BindScreenFluidQuadGeometry(renderer, particle_buffers);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei) particle_buffers->particle_count);
    glDisable(GL_DEPTH_TEST);
    SimulationRenderer_EndGpuTimerQuery(renderer->gpu_surface_inputs_query_identifiers[gpu_query_identifier_index]);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.surface_inputs_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

    QueryPerformanceCounter(&pass_start_counter);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_blur_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_comp_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glDisable(GL_DEPTH_TEST);
    glClearColor(10000.0f, 0.0f, 0.0f, 10000.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderer->screen_fluid_prepare_program_identifier);
    glUniform1i(renderer->screen_fluid_prepare_depth_texture_uniform, 0);
    glUniform1i(renderer->screen_fluid_prepare_thickness_texture_uniform, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_thickness_texture_identifier);
    glBindVertexArray(renderer->fullscreen_vao_identifier);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.prepare_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

    QueryPerformanceCounter(&pass_start_counter);
    SimulationRenderer_BeginGpuTimerQuery(renderer->gpu_blur_query_identifiers[gpu_query_identifier_index]);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_comp_blur_texture_identifier, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(10000.0f, 0.0f, 0.0f, 10000.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderer->screen_fluid_blur_program_identifier);
    glUniform1i(renderer->screen_fluid_blur_texture_uniform, 0);
    glUniform1i(
        renderer->screen_fluid_blur_filter_mode_uniform,
        screen_fluid_smoothing_mode == SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN ? 1 :
        (screen_fluid_smoothing_mode == SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D ? 2 : 0));
    glUniform1i(renderer->screen_fluid_blur_depth_texture_uniform, 1);
    glUniform2f(renderer->screen_fluid_blur_texel_size_uniform, texel_size.x, texel_size.y);
    glUniform1f(renderer->screen_fluid_blur_projection_scale_uniform, projection_matrix.elements[0]);
    glUniform1f(renderer->screen_fluid_blur_image_width_uniform, (f32) renderer->screen_fluid_texture_width);
    glUniform1f(renderer->screen_fluid_blur_world_radius_uniform, blur_world_radius);
    glUniform1i(renderer->screen_fluid_blur_max_radius_uniform, blur_max_screen_radius);
    glUniform1f(renderer->screen_fluid_blur_strength_uniform, blur_strength);
    glUniform1f(renderer->screen_fluid_blur_difference_strength_uniform, blur_difference_strength);
    glBindVertexArray(renderer->fullscreen_vao_identifier);

    if (screen_fluid_smoothing_mode == SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_comp_blur_texture_identifier, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform2f(renderer->screen_fluid_blur_direction_uniform, 0.0f, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_texture_identifier);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        final_comp_texture_identifier = renderer->screen_fluid_comp_blur_texture_identifier;
    }
    else
    {
        for (i32 blur_iteration_index = 0; blur_iteration_index < blur_iteration_count; blur_iteration_index++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_framebuffer_identifier);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_comp_blur_texture_identifier, 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glClear(GL_COLOR_BUFFER_BIT);
            glUniform2f(renderer->screen_fluid_blur_direction_uniform, 1.0f, 0.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_texture_identifier);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glBindFramebuffer(GL_FRAMEBUFFER, renderer->screen_fluid_blur_framebuffer_identifier);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_comp_texture_identifier, 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glClear(GL_COLOR_BUFFER_BIT);
            glUniform2f(renderer->screen_fluid_blur_direction_uniform, 0.0f, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_blur_texture_identifier);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
    SimulationRenderer_EndGpuTimerQuery(renderer->gpu_blur_query_identifiers[gpu_query_identifier_index]);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.blur_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);
    QueryPerformanceCounter(&pass_start_counter);
    SimulationRenderer_BeginGpuTimerQuery(renderer->gpu_normal_query_identifiers[gpu_query_identifier_index]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->screen_fluid_normal_texture_identifier, 0);
    glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderer->screen_fluid_normal_program_identifier);
    glUniform1i(renderer->screen_fluid_normal_comp_texture_uniform, 0);
    glUniform2f(renderer->screen_fluid_normal_texel_size_uniform, texel_size.x, texel_size.y);
    glUniformMatrix4fv(renderer->screen_fluid_normal_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->screen_fluid_normal_inverse_projection_uniform, 1, GL_FALSE, inverse_projection_matrix.elements);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, final_comp_texture_identifier);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    SimulationRenderer_EndGpuTimerQuery(renderer->gpu_normal_query_identifiers[gpu_query_identifier_index]);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.normal_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewport_width, viewport_height);
    QueryPerformanceCounter(&pass_start_counter);
    SimulationRenderer_BeginGpuTimerQuery(renderer->gpu_composite_query_identifiers[gpu_query_identifier_index]);
    if (screen_fluid_visualization_mode == SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED ||
        screen_fluid_visualization_mode == SIMULATION_SCREEN_FLUID_VISUALIZATION_NORMAL)
    {
        glUseProgram(renderer->screen_fluid_debug_program_identifier);
        glUniform1i(renderer->screen_fluid_debug_texture_uniform, 0);
        glUniform1i(
            renderer->screen_fluid_debug_mode_uniform,
            screen_fluid_visualization_mode == SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED ? 1 : 2);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(
            GL_TEXTURE_2D,
            screen_fluid_visualization_mode == SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED ?
                final_comp_texture_identifier :
                renderer->screen_fluid_normal_texture_identifier);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
    {
        glUseProgram(renderer->screen_fluid_composite_program_identifier);
        glUniform1i(renderer->screen_fluid_composite_texture_uniform, 0);
        glUniform1i(renderer->screen_fluid_composite_depth_texture_uniform, 1);
        glUniform1i(renderer->screen_fluid_composite_normal_texture_uniform, 2);
        glUniform1i(renderer->screen_fluid_composite_scene_texture_uniform, 3);
        glUniform1i(renderer->screen_fluid_composite_foam_texture_uniform, 4);
        glUniform1i(renderer->screen_fluid_composite_shadow_texture_uniform, 5);
        glUniformMatrix4fv(renderer->screen_fluid_composite_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
        glUniformMatrix4fv(renderer->screen_fluid_composite_inverse_projection_uniform, 1, GL_FALSE, inverse_projection_matrix.elements);
        glUniformMatrix4fv(renderer->screen_fluid_composite_inverse_view_uniform, 1, GL_FALSE, inverse_view_matrix.elements);
        glUniformMatrix4fv(
            renderer->screen_fluid_composite_shadow_view_projection_uniform,
            1,
            GL_FALSE,
            light_matrices.view_projection_matrix.elements);
        glUniform3f(renderer->screen_fluid_composite_bounds_size_uniform, simulation_bounds_size.x, simulation_bounds_size.y, simulation_bounds_size.z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, final_comp_texture_identifier);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_normal_texture_identifier);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_scene_texture_identifier);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_foam_texture_identifier);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_shadow_texture_identifier);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    SimulationRenderer_EndGpuTimerQuery(renderer->gpu_composite_query_identifiers[gpu_query_identifier_index]);
    QueryPerformanceCounter(&pass_end_counter);
    renderer->last_debug_timings.composite_milliseconds =
        SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);
}

static void SimulationRenderer_DrawBounds (SimulationRenderer *renderer, Mat4 projection_matrix, Mat4 view_matrix, Mat4 model_matrix)
{
    glDisable(GL_DEPTH_TEST);
    glUseProgram(renderer->line_program_identifier);
    glUniformMatrix4fv(renderer->line_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->line_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glUniformMatrix4fv(renderer->line_model_uniform, 1, GL_FALSE, model_matrix.elements);
    glUniform4f(renderer->line_color_uniform, 0.85f, 0.88f, 0.95f, 1.0f);
    glBindVertexArray(renderer->bounds_vao_identifier);
    glDrawArrays(GL_LINES, 0, 24);
}

static void SimulationRenderer_DrawWhitewaterToFoamTexture (
    SimulationRenderer *renderer,
    const SimulationWhitewater *whitewater,
    Mat4 projection_matrix,
    Mat4 view_matrix)
{
    Base_Assert(renderer != NULL);
    Base_Assert(whitewater != NULL);

    if (whitewater->maximum_particle_count == 0) return;

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(renderer->whitewater_program_identifier);
    glUniformMatrix4fv(renderer->whitewater_projection_uniform, 1, GL_FALSE, projection_matrix.elements);
    glUniformMatrix4fv(renderer->whitewater_view_uniform, 1, GL_FALSE, view_matrix.elements);
    glBindVertexArray(renderer->whitewater_vao_identifier);
    glDrawArrays(GL_POINTS, 0, (GLsizei) whitewater->maximum_particle_count);
}

void SimulationRenderer_LogScreenFluidReadback (SimulationRenderer *renderer)
{
    f32 *texture_data;
    i32 pixel_count;
    i32 float_count;
    i32 non_zero_channel_count;
    f32 maximum_alpha_value;
    f32 maximum_color_value;
    f32 minimum_primary_value;

    Base_Assert(renderer != NULL);

    if (renderer->screen_fluid_texture_width <= 0 ||
        renderer->screen_fluid_texture_height <= 0 ||
        renderer->screen_fluid_comp_texture_identifier == 0 ||
        renderer->screen_fluid_comp_blur_texture_identifier == 0)
    {
        Base_LogInfo("Screen fluid targets are not allocated.");
        return;
    }

    pixel_count = renderer->screen_fluid_texture_width * renderer->screen_fluid_texture_height;
    float_count = pixel_count * 4;
    texture_data = (f32 *) malloc((size_t) float_count * sizeof(f32));
    if (texture_data == NULL)
    {
        Base_LogError("Failed to allocate screen fluid readback buffer.");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_texture_identifier);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, texture_data);
    maximum_alpha_value = 0.0f;
    maximum_color_value = 0.0f;
    minimum_primary_value = 10000.0f;
    non_zero_channel_count = 0;
    for (i32 pixel_index = 0; pixel_index < pixel_count; pixel_index++)
    {
        f32 red_value = texture_data[pixel_index * 4 + 0];
        f32 green_value = texture_data[pixel_index * 4 + 1];
        f32 blue_value = texture_data[pixel_index * 4 + 2];
        f32 alpha_value = texture_data[pixel_index * 4 + 3];
        f32 color_value = red_value;
        if (green_value > color_value) color_value = green_value;
        if (blue_value > color_value) color_value = blue_value;
        if (alpha_value > color_value) color_value = alpha_value;
        if (color_value > 0.00001f) non_zero_channel_count++;
        if (red_value < minimum_primary_value) minimum_primary_value = red_value;
        if (alpha_value > maximum_alpha_value) maximum_alpha_value = alpha_value;
        if (color_value > maximum_color_value) maximum_color_value = color_value;
    }
    Base_LogInfo(
        "Screen fluid packed readback: %d x %d, non-zero pixels = %d, min smooth depth = %.5f, max hard depth = %.5f, max channel = %.5f",
        renderer->screen_fluid_texture_width,
        renderer->screen_fluid_texture_height,
        non_zero_channel_count,
        minimum_primary_value,
        maximum_alpha_value,
        maximum_color_value);

    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_comp_blur_texture_identifier);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, texture_data);
    maximum_alpha_value = 0.0f;
    maximum_color_value = 0.0f;
    minimum_primary_value = 10000.0f;
    non_zero_channel_count = 0;
    for (i32 pixel_index = 0; pixel_index < pixel_count; pixel_index++)
    {
        f32 red_value = texture_data[pixel_index * 4 + 0];
        f32 green_value = texture_data[pixel_index * 4 + 1];
        f32 blue_value = texture_data[pixel_index * 4 + 2];
        f32 alpha_value = texture_data[pixel_index * 4 + 3];
        f32 color_value = red_value;
        if (green_value > color_value) color_value = green_value;
        if (blue_value > color_value) color_value = blue_value;
        if (alpha_value > color_value) color_value = alpha_value;
        if (color_value > 0.00001f) non_zero_channel_count++;
        if (red_value < minimum_primary_value) minimum_primary_value = red_value;
        if (alpha_value > maximum_alpha_value) maximum_alpha_value = alpha_value;
        if (color_value > maximum_color_value) maximum_color_value = color_value;
    }
    Base_LogInfo(
        "Screen fluid packed blur readback: %d x %d, non-zero pixels = %d, min smooth depth = %.5f, max hard depth = %.5f, max channel = %.5f",
        renderer->screen_fluid_texture_width,
        renderer->screen_fluid_texture_height,
        non_zero_channel_count,
        minimum_primary_value,
        maximum_alpha_value,
        maximum_color_value);

    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_depth_texture_identifier);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, texture_data);
    maximum_color_value = 0.0f;
    minimum_primary_value = 10000.0f;
    non_zero_channel_count = 0;
    for (i32 pixel_index = 0; pixel_index < pixel_count; pixel_index++)
    {
        f32 depth_value = texture_data[pixel_index];
        if (depth_value > 0.00001f) non_zero_channel_count++;
        if (depth_value < minimum_primary_value) minimum_primary_value = depth_value;
        if (depth_value > maximum_color_value) maximum_color_value = depth_value;
    }
    Base_LogInfo(
        "Screen fluid depth readback: %d x %d, non-zero pixels = %d, min depth = %.5f, max depth = %.5f",
        renderer->screen_fluid_texture_width,
        renderer->screen_fluid_texture_height,
        non_zero_channel_count,
        minimum_primary_value,
        maximum_color_value);

    glBindTexture(GL_TEXTURE_2D, renderer->screen_fluid_normal_texture_identifier);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, texture_data);
    maximum_color_value = 0.0f;
    minimum_primary_value = 10000.0f;
    non_zero_channel_count = 0;
    for (i32 pixel_index = 0; pixel_index < pixel_count; pixel_index++)
    {
        f32 red_value = texture_data[pixel_index * 4 + 0];
        f32 green_value = texture_data[pixel_index * 4 + 1];
        f32 blue_value = texture_data[pixel_index * 4 + 2];
        f32 alpha_value = texture_data[pixel_index * 4 + 3];
        f32 color_value = red_value;
        if (green_value > color_value) color_value = green_value;
        if (blue_value > color_value) color_value = blue_value;
        if (alpha_value > color_value) color_value = alpha_value;
        if (color_value > 0.00001f) non_zero_channel_count++;
        if (red_value < minimum_primary_value) minimum_primary_value = red_value;
        if (color_value > maximum_color_value) maximum_color_value = color_value;
    }
    Base_LogInfo(
        "Screen fluid normal readback: %d x %d, non-zero pixels = %d, min red = %.5f, max channel = %.5f",
        renderer->screen_fluid_texture_width,
        renderer->screen_fluid_texture_height,
        non_zero_channel_count,
        minimum_primary_value,
        maximum_color_value);

    glBindTexture(GL_TEXTURE_2D, 0);
    free(texture_data);
}

void SimulationRenderer_Render (
    SimulationRenderer *renderer,
    const SimulationParticleBuffers *particle_buffers,
    const SimulationWhitewater *whitewater,
    const SimulationVolumeDensity *volume_density,
    SimulationCamera camera,
    Vec3 simulation_bounds_size,
    SimulationRenderMode render_mode,
    SimulationParticleVisualizationMode particle_visualization_mode,
    SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode,
    SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode,
    f32 density_minimum,
    f32 density_maximum,
    f32 speed_minimum,
    f32 speed_maximum,
    i32 viewport_width,
    i32 viewport_height)
{
    LARGE_INTEGER performance_frequency = {0};
    LARGE_INTEGER total_start_counter = {0};
    LARGE_INTEGER total_end_counter = {0};
    LARGE_INTEGER pass_start_counter = {0};
    LARGE_INTEGER pass_end_counter = {0};
    f32 aspect_ratio;
    Mat4 projection_matrix;
    Mat4 bounds_model_matrix;
    SimulationCameraMatrices camera_matrices;
    u32 gpu_query_identifier_index;

    Base_Assert(renderer != NULL);
    Base_Assert(particle_buffers != NULL);
    Base_Assert(whitewater != NULL);
    renderer->last_debug_timings.bounds_milliseconds = 0.0;
    renderer->last_debug_timings.particles_milliseconds = 0.0;
    renderer->last_debug_timings.scene_copy_milliseconds = 0.0;
    renderer->last_debug_timings.shadow_milliseconds = 0.0;
    renderer->last_debug_timings.surface_inputs_milliseconds = 0.0;
    renderer->last_debug_timings.prepare_milliseconds = 0.0;
    renderer->last_debug_timings.blur_milliseconds = 0.0;
    renderer->last_debug_timings.normal_milliseconds = 0.0;
    renderer->last_debug_timings.composite_milliseconds = 0.0;
    renderer->last_debug_timings.whitewater_milliseconds = 0.0;
    renderer->last_debug_timings.total_milliseconds = 0.0;
    gpu_query_identifier_index = renderer->gpu_query_frame_index;
    SimulationRenderer_ReadGpuTimerQuery(
        renderer->gpu_scene_copy_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_scene_copy_milliseconds);
    SimulationRenderer_ReadGpuTimerQuery(
        renderer->gpu_shadow_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_shadow_milliseconds);
    SimulationRenderer_ReadGpuTimerQuery(
        renderer->gpu_surface_inputs_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_surface_inputs_milliseconds);
    SimulationRenderer_ReadGpuTimerQuery(
        renderer->gpu_blur_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_blur_milliseconds);
    SimulationRenderer_ReadGpuTimerQuery(
        renderer->gpu_normal_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_normal_milliseconds);
    SimulationRenderer_ReadGpuTimerQuery(
        renderer->gpu_composite_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_composite_milliseconds);
    SimulationRenderer_ReadGpuTimestampRange(
        renderer->gpu_total_start_query_identifiers[gpu_query_identifier_index],
        renderer->gpu_total_end_query_identifiers[gpu_query_identifier_index],
        &renderer->last_debug_timings.gpu_total_milliseconds);
    QueryPerformanceFrequency(&performance_frequency);
    QueryPerformanceCounter(&total_start_counter);

    aspect_ratio = (viewport_height > 0) ? ((f32) viewport_width / (f32) viewport_height) : 1.0f;
    projection_matrix = Mat4_Perspective(60.0f * BASE_PI32 / 180.0f, aspect_ratio, 0.1f, 100.0f);
    camera_matrices = SimulationRenderer_CreateCameraMatrices(camera);
    bounds_model_matrix = Mat4_Scale(simulation_bounds_size.x, simulation_bounds_size.y, simulation_bounds_size.z);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewport_width, viewport_height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.43f, 0.50f, 0.61f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (render_mode == SIMULATION_RENDER_MODE_SCREEN_FLUID)
    {
        glQueryCounter(renderer->gpu_total_start_query_identifiers[gpu_query_identifier_index], GL_TIMESTAMP);
        QueryPerformanceCounter(&pass_start_counter);
        SimulationRenderer_DrawBounds(renderer, projection_matrix, camera_matrices.view_matrix, bounds_model_matrix);
        QueryPerformanceCounter(&pass_end_counter);
        renderer->last_debug_timings.bounds_milliseconds =
            SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        SimulationRenderer_DrawScreenFluid(
            renderer,
            particle_buffers,
            whitewater,
            projection_matrix,
            camera_matrices.view_matrix,
            camera_matrices.inverse_view_matrix,
            simulation_bounds_size,
            camera_matrices.position,
            screen_fluid_visualization_mode,
            screen_fluid_smoothing_mode,
            viewport_width,
            viewport_height);
        QueryPerformanceCounter(&pass_end_counter);
        renderer->last_debug_timings.total_milliseconds =
            SimulationRenderer_GetMillisecondsBetweenCounters(total_start_counter, pass_end_counter, performance_frequency);
        glQueryCounter(renderer->gpu_total_end_query_identifiers[gpu_query_identifier_index], GL_TIMESTAMP);
        renderer->last_debug_timings.whitewater_milliseconds = 0.0;
    }
    else
    {
        QueryPerformanceCounter(&pass_start_counter);
        SimulationRenderer_DrawParticles(renderer, particle_buffers, projection_matrix, camera_matrices.view_matrix, particle_visualization_mode, density_minimum, density_maximum, speed_minimum, speed_maximum);
        QueryPerformanceCounter(&pass_end_counter);
        renderer->last_debug_timings.particles_milliseconds =
            SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&pass_start_counter);
        SimulationRenderer_DrawBounds(renderer, projection_matrix, camera_matrices.view_matrix, bounds_model_matrix);
        QueryPerformanceCounter(&pass_end_counter);
        renderer->last_debug_timings.bounds_milliseconds =
            SimulationRenderer_GetMillisecondsBetweenCounters(pass_start_counter, pass_end_counter, performance_frequency);

        QueryPerformanceCounter(&total_end_counter);
        renderer->last_debug_timings.total_milliseconds =
            SimulationRenderer_GetMillisecondsBetweenCounters(total_start_counter, total_end_counter, performance_frequency);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    if (render_mode == SIMULATION_RENDER_MODE_SCREEN_FLUID)
    {
        QueryPerformanceCounter(&total_end_counter);
        renderer->last_debug_timings.total_milliseconds =
            SimulationRenderer_GetMillisecondsBetweenCounters(total_start_counter, total_end_counter, performance_frequency);
    }
    renderer->gpu_query_frame_index = (renderer->gpu_query_frame_index + 1u) % SIMULATION_RENDERER_GPU_QUERY_RING_SIZE;
}
