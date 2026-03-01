#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "opengl_helpers.h"

typedef void (APIENTRYP GlDispatchComputeFunction)(GLuint group_count_x, GLuint group_count_y, GLuint group_count_z);
typedef void (APIENTRYP GlMemoryBarrierFunction)(GLbitfield barriers);
typedef void (APIENTRYP GlBindImageTextureFunction)(
    GLuint unit,
    GLuint texture,
    GLint level,
    GLboolean layered,
    GLint layer,
    GLenum access,
    GLenum format);

static GlDispatchComputeFunction gl_dispatch_compute = NULL;
static GlMemoryBarrierFunction gl_memory_barrier = NULL;
static GlBindImageTextureFunction gl_bind_image_texture = NULL;

static void *OpenGL_LoadProcedureAddress (const char *procedure_name)
{
    void *procedure_address = (void *) wglGetProcAddress(procedure_name);
    if (procedure_address == NULL ||
        procedure_address == (void *) 0x1 ||
        procedure_address == (void *) 0x2 ||
        procedure_address == (void *) 0x3 ||
        procedure_address == (void *) -1)
    {
        HMODULE opengl_library = GetModuleHandleA("opengl32.dll");
        if (opengl_library != NULL)
        {
            procedure_address = (void *) GetProcAddress(opengl_library, procedure_name);
        }
    }

    return procedure_address;
}

static char *OpenGL_ReadTextFile (const char *file_path, i32 *file_size)
{
    FILE *file_handle = fopen(file_path, "rb");
    if (file_handle == NULL)
    {
        Base_LogError("Failed to open shader file: %s", file_path);
        return NULL;
    }

    if (fseek(file_handle, 0, SEEK_END) != 0)
    {
        fclose(file_handle);
        Base_LogError("Failed to seek shader file: %s", file_path);
        return NULL;
    }

    long size_in_bytes = ftell(file_handle);
    if (size_in_bytes < 0)
    {
        fclose(file_handle);
        Base_LogError("Failed to query shader file size: %s", file_path);
        return NULL;
    }

    rewind(file_handle);

    char *file_data = (char *) malloc((size_t) size_in_bytes + 1);
    if (file_data == NULL)
    {
        fclose(file_handle);
        Base_LogError("Failed to allocate memory for shader file: %s", file_path);
        return NULL;
    }

    size_t read_size = fread(file_data, 1, (size_t) size_in_bytes, file_handle);
    fclose(file_handle);

    if (read_size != (size_t) size_in_bytes)
    {
        free(file_data);
        Base_LogError("Failed to read full shader file: %s", file_path);
        return NULL;
    }

    file_data[size_in_bytes] = '\0';

    if (file_size != NULL)
    {
        *file_size = (i32) size_in_bytes;
    }

    return file_data;
}

static void OpenGL_LogShaderCompilationError (u32 shader_identifier, const char *debug_name)
{
    GLint log_length = 0;
    glGetShaderiv(shader_identifier, GL_INFO_LOG_LENGTH, &log_length);

    if (log_length <= 1)
    {
        Base_LogError("Shader compilation failed: %s", debug_name);
        return;
    }

    char *info_log = (char *) malloc((size_t) log_length);
    if (info_log == NULL)
    {
        Base_LogError("Shader compilation failed and log allocation failed: %s", debug_name);
        return;
    }

    glGetShaderInfoLog(shader_identifier, log_length, NULL, info_log);
    Base_LogError("Shader compilation failed: %s\n%s", debug_name, info_log);
    free(info_log);
}

static void OpenGL_LogProgramLinkError (u32 program_identifier, const char *debug_name)
{
    GLint log_length = 0;
    glGetProgramiv(program_identifier, GL_INFO_LOG_LENGTH, &log_length);

    if (log_length <= 1)
    {
        Base_LogError("Program link failed: %s", debug_name);
        return;
    }

    char *info_log = (char *) malloc((size_t) log_length);
    if (info_log == NULL)
    {
        Base_LogError("Program link failed and log allocation failed: %s", debug_name);
        return;
    }

    glGetProgramInfoLog(program_identifier, log_length, NULL, info_log);
    Base_LogError("Program link failed: %s\n%s", debug_name, info_log);
    free(info_log);
}

bool OpenGL_LoadComputeFunctions (void)
{
    gl_dispatch_compute = (GlDispatchComputeFunction) OpenGL_LoadProcedureAddress("glDispatchCompute");
    gl_memory_barrier = (GlMemoryBarrierFunction) OpenGL_LoadProcedureAddress("glMemoryBarrier");

    if (gl_dispatch_compute == NULL || gl_memory_barrier == NULL)
    {
        Base_LogError("Failed to load required compute shader functions.");
        return false;
    }

    return true;
}

bool OpenGL_ComputeFunctionsAreAvailable (void)
{
    return gl_dispatch_compute != NULL && gl_memory_barrier != NULL;
}

void OpenGL_DispatchCompute (u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    Base_Assert(gl_dispatch_compute != NULL);
    gl_dispatch_compute(group_count_x, group_count_y, group_count_z);
}

void OpenGL_MemoryBarrier (u32 barrier_flags)
{
    Base_Assert(gl_memory_barrier != NULL);
    gl_memory_barrier(barrier_flags);
}

bool OpenGL_LoadImageFunctions (void)
{
    gl_bind_image_texture = (GlBindImageTextureFunction) OpenGL_LoadProcedureAddress("glBindImageTexture");

    if (gl_bind_image_texture == NULL)
    {
        Base_LogError("Failed to load required image texture functions.");
        return false;
    }

    return true;
}

bool OpenGL_ImageFunctionsAreAvailable (void)
{
    return gl_bind_image_texture != NULL;
}

void OpenGL_BindImageTexture (
    u32 unit_index,
    u32 texture_identifier,
    i32 mip_level,
    bool is_layered,
    i32 layer_index,
    u32 access,
    u32 image_format)
{
    Base_Assert(gl_bind_image_texture != NULL);
    gl_bind_image_texture(
        unit_index,
        texture_identifier,
        mip_level,
        is_layered ? GL_TRUE : GL_FALSE,
        layer_index,
        access,
        image_format);
}

u32 OpenGL_CompileShaderFromSource (u32 shader_type, const char *shader_source, const char *debug_name)
{
    Base_Assert(shader_source != NULL);
    Base_Assert(debug_name != NULL);

    u32 shader_identifier = glCreateShader(shader_type);
    if (shader_identifier == 0)
    {
        Base_LogError("Failed to create shader object: %s", debug_name);
        return 0;
    }

    glShaderSource(shader_identifier, 1, &shader_source, NULL);
    glCompileShader(shader_identifier);

    GLint compile_status = GL_FALSE;
    glGetShaderiv(shader_identifier, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE)
    {
        OpenGL_LogShaderCompilationError(shader_identifier, debug_name);
        glDeleteShader(shader_identifier);
        return 0;
    }

    return shader_identifier;
}

u32 OpenGL_CompileShaderFromFile (u32 shader_type, const char *file_path)
{
    Base_Assert(file_path != NULL);

    i32 file_size = 0;
    char *shader_source = OpenGL_ReadTextFile(file_path, &file_size);
    if (shader_source == NULL)
    {
        return 0;
    }

    u32 shader_identifier = OpenGL_CompileShaderFromSource(shader_type, shader_source, file_path);
    free(shader_source);
    return shader_identifier;
}

u32 OpenGL_LinkProgram (const u32 *shader_identifiers, u32 shader_count, const char *debug_name)
{
    Base_Assert(shader_identifiers != NULL);
    Base_Assert(debug_name != NULL);

    u32 program_identifier = glCreateProgram();
    if (program_identifier == 0)
    {
        Base_LogError("Failed to create program object: %s", debug_name);
        return 0;
    }

    for (u32 shader_index = 0; shader_index < shader_count; shader_index++)
    {
        glAttachShader(program_identifier, shader_identifiers[shader_index]);
    }

    glLinkProgram(program_identifier);

    GLint link_status = GL_FALSE;
    glGetProgramiv(program_identifier, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE)
    {
        OpenGL_LogProgramLinkError(program_identifier, debug_name);
        glDeleteProgram(program_identifier);
        return 0;
    }

    for (u32 shader_index = 0; shader_index < shader_count; shader_index++)
    {
        glDetachShader(program_identifier, shader_identifiers[shader_index]);
    }

    return program_identifier;
}

u32 OpenGL_CreateComputeProgramFromFile (const char *file_path)
{
    u32 compute_shader_identifier = OpenGL_CompileShaderFromFile(GL_COMPUTE_SHADER, file_path);
    if (compute_shader_identifier == 0)
    {
        return 0;
    }

    u32 program_identifier = OpenGL_LinkProgram(&compute_shader_identifier, 1, file_path);
    glDeleteShader(compute_shader_identifier);
    return program_identifier;
}

bool OpenGL_CreateBuffer (OpenGLBuffer *buffer, u32 target, i32 size_in_bytes, const void *initial_data, u32 usage)
{
    Base_Assert(buffer != NULL);

    memset(buffer, 0, sizeof(*buffer));
    glGenBuffers(1, &buffer->identifier);
    if (buffer->identifier == 0)
    {
        Base_LogError("Failed to create OpenGL buffer.");
        return false;
    }

    buffer->target = target;
    buffer->size_in_bytes = size_in_bytes;

    glBindBuffer(target, buffer->identifier);
    glBufferData(target, size_in_bytes, initial_data, usage);
    glBindBuffer(target, 0);
    return true;
}

void OpenGL_DestroyBuffer (OpenGLBuffer *buffer)
{
    if (buffer == NULL) return;
    if (buffer->identifier != 0)
    {
        glDeleteBuffers(1, &buffer->identifier);
    }

    buffer->identifier = 0;
    buffer->target = 0;
    buffer->size_in_bytes = 0;
}

bool OpenGL_UpdateBuffer (OpenGLBuffer *buffer, i32 offset_in_bytes, i32 size_in_bytes, const void *data)
{
    Base_Assert(buffer != NULL);
    Base_Assert(data != NULL);

    if (offset_in_bytes < 0 || size_in_bytes < 0 || offset_in_bytes + size_in_bytes > buffer->size_in_bytes)
    {
        Base_LogError("OpenGL buffer update out of range.");
        return false;
    }

    glBindBuffer(buffer->target, buffer->identifier);
    glBufferSubData(buffer->target, offset_in_bytes, size_in_bytes, data);
    glBindBuffer(buffer->target, 0);
    return true;
}

bool OpenGL_ReadBuffer (OpenGLBuffer *buffer, void *destination, i32 size_in_bytes)
{
    Base_Assert(buffer != NULL);
    Base_Assert(destination != NULL);

    if (size_in_bytes < 0 || size_in_bytes > buffer->size_in_bytes)
    {
        Base_LogError("OpenGL buffer read out of range.");
        return false;
    }

    glBindBuffer(buffer->target, buffer->identifier);
    glGetBufferSubData(buffer->target, 0, size_in_bytes, destination);
    glBindBuffer(buffer->target, 0);
    return true;
}

bool OpenGL_CreateTexture3D (
    OpenGLTexture3D *texture,
    i32 width,
    i32 height,
    i32 depth,
    u32 internal_format,
    u32 format,
    u32 type,
    const void *initial_data)
{
    Base_Assert(texture != NULL);

    memset(texture, 0, sizeof(*texture));
    if (width <= 0 || height <= 0 || depth <= 0)
    {
        Base_LogError("OpenGL 3D texture dimensions must be positive.");
        return false;
    }

    glGenTextures(1, &texture->identifier);
    if (texture->identifier == 0)
    {
        Base_LogError("Failed to create OpenGL 3D texture.");
        return false;
    }

    texture->width = width;
    texture->height = height;
    texture->depth = depth;
    texture->internal_format = internal_format;

    glBindTexture(GL_TEXTURE_3D, texture->identifier);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, (GLint) internal_format, width, height, depth, 0, format, type, initial_data);
    glBindTexture(GL_TEXTURE_3D, 0);
    return true;
}

void OpenGL_DestroyTexture3D (OpenGLTexture3D *texture)
{
    if (texture == NULL) return;

    if (texture->identifier != 0)
    {
        glDeleteTextures(1, &texture->identifier);
    }

    memset(texture, 0, sizeof(*texture));
}

bool OpenGL_ClearTexture3D (OpenGLTexture3D *texture, u32 format, u32 type, const void *clear_value)
{
    Base_Assert(texture != NULL);
    Base_Assert(clear_value != NULL);
    (void) clear_value;

    if (texture->identifier == 0)
    {
        Base_LogError("Cannot clear an uninitialized OpenGL 3D texture.");
        return false;
    }

    glBindTexture(GL_TEXTURE_3D, texture->identifier);
    glTexImage3D(
        GL_TEXTURE_3D,
        0,
        (GLint) texture->internal_format,
        texture->width,
        texture->height,
        texture->depth,
        0,
        format,
        type,
        NULL);
    glBindTexture(GL_TEXTURE_3D, 0);
    return true;
}

bool OpenGL_ReadTexture3D (
    OpenGLTexture3D *texture,
    u32 format,
    u32 type,
    void *destination,
    i32 destination_size_in_bytes)
{
    Base_Assert(texture != NULL);
    Base_Assert(destination != NULL);

    if (texture->identifier == 0)
    {
        Base_LogError("Cannot read an uninitialized OpenGL 3D texture.");
        return false;
    }

    if (destination_size_in_bytes <= 0)
    {
        Base_LogError("OpenGL 3D texture read size must be positive.");
        return false;
    }

    glBindTexture(GL_TEXTURE_3D, texture->identifier);
    glGetTexImage(GL_TEXTURE_3D, 0, format, type, destination);
    glBindTexture(GL_TEXTURE_3D, 0);
    return true;
}

bool OpenGL_RunComputeValidation (void)
{
    if (GLVersion.major < 4 || (GLVersion.major == 4 && GLVersion.minor < 3))
    {
        Base_LogError("OpenGL 4.3 or newer is required for compute validation. Detected version: %d.%d", GLVersion.major, GLVersion.minor);
        return false;
    }

    if (!OpenGL_LoadComputeFunctions())
    {
        return false;
    }

    const char *compute_shader_path = "assets/shaders/validation_compute.glsl";
    u32 compute_program = OpenGL_CreateComputeProgramFromFile(compute_shader_path);
    if (compute_program == 0)
    {
        return false;
    }

    enum { validation_value_count = 8 };
    u32 initial_values[validation_value_count] = {0};
    u32 result_values[validation_value_count] = {0};

    OpenGLBuffer storage_buffer = {0};
    if (!OpenGL_CreateBuffer(
        &storage_buffer,
        GL_SHADER_STORAGE_BUFFER,
        (i32) sizeof(initial_values),
        initial_values,
        GL_DYNAMIC_COPY))
    {
        glDeleteProgram(compute_program);
        return false;
    }

    glUseProgram(compute_program);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, storage_buffer.identifier);
    OpenGL_DispatchCompute(validation_value_count, 1, 1);
    OpenGL_MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    bool read_success = OpenGL_ReadBuffer(&storage_buffer, result_values, (i32) sizeof(result_values));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glUseProgram(0);

    OpenGL_DestroyBuffer(&storage_buffer);
    glDeleteProgram(compute_program);

    if (!read_success)
    {
        return false;
    }

    for (u32 value_index = 0; value_index < validation_value_count; value_index++)
    {
        u32 expected_value = value_index * value_index + 7;
        if (result_values[value_index] != expected_value)
        {
            Base_LogError(
                "Compute validation failed at index %u. Expected %u, received %u.",
                value_index,
                expected_value,
                result_values[value_index]);
            return false;
        }
    }

    Base_LogInfo("Compute validation passed.");
    return true;
}
