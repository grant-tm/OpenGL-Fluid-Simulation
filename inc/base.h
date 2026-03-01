#ifndef BASE_H
#define BASE_H

#include <stdbool.h>
#include <stdint.h>

#define BASE_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef struct Vec2
{
    f32 x;
    f32 y;
} Vec2;

typedef struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
} Vec3;

typedef struct Vec4
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
} Vec4;

typedef struct Mat4
{
    f32 elements[16];
} Mat4;

extern const f32 BASE_PI32;
extern const f32 BASE_EPSILON32;

void Base_LogInfo (const char *format, ...);
void Base_LogError (const char *format, ...);
void Base_Abort (const char *expression, const char *file_path, i32 line_number);
bool Base_RunSelfChecks (void);

#define Base_Assert(expression) ((expression) ? (void) 0 : Base_Abort(#expression, __FILE__, __LINE__))

f32 Base_ClampF32 (f32 value, f32 minimum_value, f32 maximum_value);
f32 Base_LerpF32 (f32 value_a, f32 value_b, f32 t);
bool Base_F32NearlyEqual (f32 value_a, f32 value_b, f32 tolerance);

Vec2 Vec2_Create (f32 x, f32 y);
Vec2 Vec2_Add (Vec2 value_a, Vec2 value_b);
Vec2 Vec2_Subtract (Vec2 value_a, Vec2 value_b);
Vec2 Vec2_Scale (Vec2 value, f32 scale);
f32 Vec2_Dot (Vec2 value_a, Vec2 value_b);
f32 Vec2_Length (Vec2 value);
Vec2 Vec2_Normalize (Vec2 value);

Vec3 Vec3_Create (f32 x, f32 y, f32 z);
Vec3 Vec3_Add (Vec3 value_a, Vec3 value_b);
Vec3 Vec3_Subtract (Vec3 value_a, Vec3 value_b);
Vec3 Vec3_Scale (Vec3 value, f32 scale);
f32 Vec3_Dot (Vec3 value_a, Vec3 value_b);
Vec3 Vec3_Cross (Vec3 value_a, Vec3 value_b);
f32 Vec3_Length (Vec3 value);
Vec3 Vec3_Normalize (Vec3 value);

Vec4 Vec4_Create (f32 x, f32 y, f32 z, f32 w);
Vec4 Vec4_Add (Vec4 value_a, Vec4 value_b);
Vec4 Vec4_Subtract (Vec4 value_a, Vec4 value_b);
Vec4 Vec4_Scale (Vec4 value, f32 scale);
f32 Vec4_Dot (Vec4 value_a, Vec4 value_b);
f32 Vec4_Length (Vec4 value);
Vec4 Vec4_Normalize (Vec4 value);

Mat4 Mat4_Identity (void);
Mat4 Mat4_Ortho (f32 left, f32 right, f32 bottom, f32 top, f32 near_plane, f32 far_plane);
Mat4 Mat4_Perspective (f32 field_of_view_radians, f32 aspect_ratio, f32 near_plane, f32 far_plane);
Mat4 Mat4_Translate (f32 x, f32 y, f32 z);
Mat4 Mat4_Scale (f32 x, f32 y, f32 z);
Mat4 Mat4_RotateZ (f32 radians);
Mat4 Mat4_Multiply (Mat4 left_matrix, Mat4 right_matrix);

#endif // BASE_H
