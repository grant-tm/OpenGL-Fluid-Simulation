#ifndef OPENGL_HELPERS_H
#define OPENGL_HELPERS_H

#include "base.h"
#include "glad.h"

#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

#ifndef GL_BUFFER_UPDATE_BARRIER_BIT
#define GL_BUFFER_UPDATE_BARRIER_BIT 0x00000200
#endif

#ifndef GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT 0x00000001
#endif

typedef struct OpenGLBuffer
{
    u32 identifier;
    u32 target;
    i32 size_in_bytes;
} OpenGLBuffer;

bool OpenGL_LoadComputeFunctions (void);
bool OpenGL_ComputeFunctionsAreAvailable (void);
void OpenGL_DispatchCompute (u32 group_count_x, u32 group_count_y, u32 group_count_z);
void OpenGL_MemoryBarrier (u32 barrier_flags);

u32 OpenGL_CompileShaderFromSource (u32 shader_type, const char *shader_source, const char *debug_name);
u32 OpenGL_CompileShaderFromFile (u32 shader_type, const char *file_path);
u32 OpenGL_LinkProgram (const u32 *shader_identifiers, u32 shader_count, const char *debug_name);
u32 OpenGL_CreateComputeProgramFromFile (const char *file_path);

bool OpenGL_CreateBuffer (OpenGLBuffer *buffer, u32 target, i32 size_in_bytes, const void *initial_data, u32 usage);
void OpenGL_DestroyBuffer (OpenGLBuffer *buffer);
bool OpenGL_UpdateBuffer (OpenGLBuffer *buffer, i32 offset_in_bytes, i32 size_in_bytes, const void *data);
bool OpenGL_ReadBuffer (OpenGLBuffer *buffer, void *destination, i32 size_in_bytes);

bool OpenGL_RunComputeValidation (void);

#endif // OPENGL_HELPERS_H
