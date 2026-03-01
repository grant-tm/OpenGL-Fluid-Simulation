#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "base.h"

const f32 BASE_PI32 = 3.14159265358979323846f;
const f32 BASE_EPSILON32 = 0.0001f;

static void Base_LogWithPrefix (const char *prefix, const char *format, va_list argument_list)
{
    fputs(prefix, stderr);
    vfprintf(stderr, format, argument_list);
    fputc('\n', stderr);
}

void Base_LogInfo (const char *format, ...)
{
    va_list argument_list;
    va_start(argument_list, format);
    Base_LogWithPrefix("[Info] ", format, argument_list);
    va_end(argument_list);
}

void Base_LogError (const char *format, ...)
{
    va_list argument_list;
    va_start(argument_list, format);
    Base_LogWithPrefix("[Error] ", format, argument_list);
    va_end(argument_list);
}

void Base_Abort (const char *expression, const char *file_path, i32 line_number)
{
    Base_LogError("Assertion failed: %s (%s:%d)", expression, file_path, line_number);
    abort();
}

f32 Base_ClampF32 (f32 value, f32 minimum_value, f32 maximum_value)
{
    if (value < minimum_value) return minimum_value;
    if (value > maximum_value) return maximum_value;
    return value;
}

f32 Base_LerpF32 (f32 value_a, f32 value_b, f32 t)
{
    return value_a + (value_b - value_a) * t;
}

bool Base_F32NearlyEqual (f32 value_a, f32 value_b, f32 tolerance)
{
    return fabsf(value_a - value_b) <= tolerance;
}

Vec2 Vec2_Create (f32 x, f32 y)
{
    Vec2 result = { x, y };
    return result;
}

Vec2 Vec2_Add (Vec2 value_a, Vec2 value_b)
{
    Vec2 result = { value_a.x + value_b.x, value_a.y + value_b.y };
    return result;
}

Vec2 Vec2_Subtract (Vec2 value_a, Vec2 value_b)
{
    Vec2 result = { value_a.x - value_b.x, value_a.y - value_b.y };
    return result;
}

Vec2 Vec2_Scale (Vec2 value, f32 scale)
{
    Vec2 result = { value.x * scale, value.y * scale };
    return result;
}

f32 Vec2_Dot (Vec2 value_a, Vec2 value_b)
{
    return value_a.x * value_b.x + value_a.y * value_b.y;
}

f32 Vec2_Length (Vec2 value)
{
    return sqrtf(Vec2_Dot(value, value));
}

Vec2 Vec2_Normalize (Vec2 value)
{
    f32 length = Vec2_Length(value);
    if (length <= BASE_EPSILON32) return value;
    return Vec2_Scale(value, 1.0f / length);
}

Vec3 Vec3_Create (f32 x, f32 y, f32 z)
{
    Vec3 result = { x, y, z };
    return result;
}

Vec3 Vec3_Add (Vec3 value_a, Vec3 value_b)
{
    Vec3 result = { value_a.x + value_b.x, value_a.y + value_b.y, value_a.z + value_b.z };
    return result;
}

Vec3 Vec3_Subtract (Vec3 value_a, Vec3 value_b)
{
    Vec3 result = { value_a.x - value_b.x, value_a.y - value_b.y, value_a.z - value_b.z };
    return result;
}

Vec3 Vec3_Scale (Vec3 value, f32 scale)
{
    Vec3 result = { value.x * scale, value.y * scale, value.z * scale };
    return result;
}

f32 Vec3_Dot (Vec3 value_a, Vec3 value_b)
{
    return value_a.x * value_b.x + value_a.y * value_b.y + value_a.z * value_b.z;
}

Vec3 Vec3_Cross (Vec3 value_a, Vec3 value_b)
{
    Vec3 result =
    {
        value_a.y * value_b.z - value_a.z * value_b.y,
        value_a.z * value_b.x - value_a.x * value_b.z,
        value_a.x * value_b.y - value_a.y * value_b.x,
    };
    return result;
}

f32 Vec3_Length (Vec3 value)
{
    return sqrtf(Vec3_Dot(value, value));
}

Vec3 Vec3_Normalize (Vec3 value)
{
    f32 length = Vec3_Length(value);
    if (length <= BASE_EPSILON32) return value;
    return Vec3_Scale(value, 1.0f / length);
}

Vec4 Vec4_Create (f32 x, f32 y, f32 z, f32 w)
{
    Vec4 result = { x, y, z, w };
    return result;
}

Vec4 Vec4_Add (Vec4 value_a, Vec4 value_b)
{
    Vec4 result =
    {
        value_a.x + value_b.x,
        value_a.y + value_b.y,
        value_a.z + value_b.z,
        value_a.w + value_b.w,
    };
    return result;
}

Vec4 Vec4_Subtract (Vec4 value_a, Vec4 value_b)
{
    Vec4 result =
    {
        value_a.x - value_b.x,
        value_a.y - value_b.y,
        value_a.z - value_b.z,
        value_a.w - value_b.w,
    };
    return result;
}

Vec4 Vec4_Scale (Vec4 value, f32 scale)
{
    Vec4 result =
    {
        value.x * scale,
        value.y * scale,
        value.z * scale,
        value.w * scale,
    };
    return result;
}

f32 Vec4_Dot (Vec4 value_a, Vec4 value_b)
{
    return value_a.x * value_b.x + value_a.y * value_b.y + value_a.z * value_b.z + value_a.w * value_b.w;
}

f32 Vec4_Length (Vec4 value)
{
    return sqrtf(Vec4_Dot(value, value));
}

Vec4 Vec4_Normalize (Vec4 value)
{
    f32 length = Vec4_Length(value);
    if (length <= BASE_EPSILON32) return value;
    return Vec4_Scale(value, 1.0f / length);
}

Mat4 Mat4_Identity (void)
{
    Mat4 result = {0};
    result.elements[0] = 1.0f;
    result.elements[5] = 1.0f;
    result.elements[10] = 1.0f;
    result.elements[15] = 1.0f;
    return result;
}

Mat4 Mat4_Ortho (f32 left, f32 right, f32 bottom, f32 top, f32 near_plane, f32 far_plane)
{
    Mat4 result = {0};
    result.elements[0] = 2.0f / (right - left);
    result.elements[5] = 2.0f / (top - bottom);
    result.elements[10] = -2.0f / (far_plane - near_plane);
    result.elements[12] = -(right + left) / (right - left);
    result.elements[13] = -(top + bottom) / (top - bottom);
    result.elements[14] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.elements[15] = 1.0f;
    return result;
}

Mat4 Mat4_Perspective (f32 field_of_view_radians, f32 aspect_ratio, f32 near_plane, f32 far_plane)
{
    f32 cotangent = 1.0f / tanf(field_of_view_radians * 0.5f);

    Mat4 result = {0};
    result.elements[0] = cotangent / aspect_ratio;
    result.elements[5] = cotangent;
    result.elements[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.elements[11] = -1.0f;
    result.elements[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

Mat4 Mat4_Translate (f32 x, f32 y, f32 z)
{
    Mat4 result = Mat4_Identity();
    result.elements[12] = x;
    result.elements[13] = y;
    result.elements[14] = z;
    return result;
}

Mat4 Mat4_Scale (f32 x, f32 y, f32 z)
{
    Mat4 result = {0};
    result.elements[0] = x;
    result.elements[5] = y;
    result.elements[10] = z;
    result.elements[15] = 1.0f;
    return result;
}

Mat4 Mat4_RotateZ (f32 radians)
{
    f32 cosine = cosf(radians);
    f32 sine = sinf(radians);

    Mat4 result = Mat4_Identity();
    result.elements[0] = cosine;
    result.elements[1] = sine;
    result.elements[4] = -sine;
    result.elements[5] = cosine;
    return result;
}

Mat4 Mat4_Multiply (Mat4 left_matrix, Mat4 right_matrix)
{
    Mat4 result = {0};

    for (i32 column_index = 0; column_index < 4; column_index++)
    {
        for (i32 row_index = 0; row_index < 4; row_index++)
        {
            result.elements[column_index * 4 + row_index] =
                left_matrix.elements[0 * 4 + row_index] * right_matrix.elements[column_index * 4 + 0] +
                left_matrix.elements[1 * 4 + row_index] * right_matrix.elements[column_index * 4 + 1] +
                left_matrix.elements[2 * 4 + row_index] * right_matrix.elements[column_index * 4 + 2] +
                left_matrix.elements[3 * 4 + row_index] * right_matrix.elements[column_index * 4 + 3];
        }
    }

    return result;
}

bool Base_RunSelfChecks (void)
{
    Vec2 normalized_vector = Vec2_Normalize(Vec2_Create(3.0f, 4.0f));
    if (!Base_F32NearlyEqual(Vec2_Length(normalized_vector), 1.0f, 0.001f))
    {
        Base_LogError("Vec2 normalization self-check failed.");
        return false;
    }

    Vec3 cross_product = Vec3_Cross(Vec3_Create(1.0f, 0.0f, 0.0f), Vec3_Create(0.0f, 1.0f, 0.0f));
    if (!Base_F32NearlyEqual(cross_product.x, 0.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(cross_product.y, 0.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(cross_product.z, 1.0f, BASE_EPSILON32))
    {
        Base_LogError("Vec3 cross product self-check failed.");
        return false;
    }

    Mat4 translation_matrix = Mat4_Translate(2.0f, 3.0f, 4.0f);
    Mat4 scale_matrix = Mat4_Scale(5.0f, 6.0f, 7.0f);
    Mat4 combined_matrix = Mat4_Multiply(translation_matrix, scale_matrix);

    if (!Base_F32NearlyEqual(combined_matrix.elements[0], 5.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(combined_matrix.elements[5], 6.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(combined_matrix.elements[10], 7.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(combined_matrix.elements[12], 2.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(combined_matrix.elements[13], 3.0f, BASE_EPSILON32) ||
        !Base_F32NearlyEqual(combined_matrix.elements[14], 4.0f, BASE_EPSILON32))
    {
        Base_LogError("Mat4 multiply self-check failed.");
        return false;
    }

    Base_LogInfo("Base self-checks passed.");
    return true;
}
