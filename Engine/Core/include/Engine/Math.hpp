#pragma once

// Vulkan uses depth range [0, 1]
// OpenGL uses [-1, 1] (GLM default)
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

// Always use radians
#define GLM_FORCE_RADIANS

// Vulkan is right-handed by convention
#define GLM_FORCE_RIGHT_HANDED

#define GLM_FORCE_EXPLICIT_CTOR

// Uncomment only if you understand alignment rules.
//
// #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
//

#if defined(_MSC_VER)
// Enable intrinsics on MSVC
#define GLM_FORCE_INTRINSICS
#endif

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/ext/matrix_clip_space.hpp>  // glm::perspective
#include <glm/ext/matrix_transform.hpp>   // glm::translate, glm::rotate, glm::scale
#include <glm/ext/scalar_constants.hpp>   // glm::pi
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace OneGame::Engine::math
{
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

using mat2 = glm::mat2;
using mat4 = glm::mat4;
using quat = glm::quat;

const float pi = glm::pi<float>();

template <typename T>
inline float dist(T a, T b)
{
    return glm::distance(a, b);
}

template <typename T>
inline T max(T a, T b)
{
    return glm::max(a, b);
}

template <typename T>
inline T min(T a, T b)
{
    return glm::min(a, b);
}

template <typename T>
inline T abs(T a)
{
    return glm::abs(a);
}

template <typename T>
inline float dist_sq(T a, T b)
{
    return glm::distance2(a, b);
}

template <typename T>
inline float len(T a)
{
    return glm::length(a);
}

template <typename T>
inline float len_sq(T a)
{
    return glm::length2(a);
}

inline float clamp(float val, float low, float high) { return glm::clamp(val, low, high); }

inline vec3 normalize(vec3 val) { return glm::normalize(val); }

inline vec2 normalize(vec2 val) { return glm::normalize(val); }

inline vec3 cross(vec3 a, vec3 b) { return glm::cross(a, b); }

inline float radians(float degrees) { return glm::radians(degrees); }

inline float degrees(float radians) { return glm::degrees(radians); }

inline mat4 identity() { return glm::identity<mat4>(); }

inline quat identity_quat() { return glm::identity<quat>(); }

inline mat4 translate(const mat4& m, const vec3& v) { return glm::translate(m, v); }

inline mat4 rotate(const mat4& m, float angleRadians, const vec3& axis) { return glm::rotate(m, angleRadians, axis); }

inline mat4 rotate(const quat& q) { return glm::mat4_cast(q); }

inline quat conjugate(const quat& q) { return glm::conjugate(q); }

inline mat4 scale(const mat4& m, const vec3& v) { return glm::scale(m, v); }

inline mat4 translate(const vec3& v) { return translate(identity(), v); }

inline mat4 rotate(float angleRadians, const vec3& axis) { return rotate(identity(), angleRadians, axis); }

inline mat4 scale(const vec3& v) { return scale(identity(), v); }

inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) { return glm::lookAt(eye, center, up); }

inline mat4 perspective(float fovRadians, float aspect, float nearPlane, float farPlane)
{
    return glm::perspective(fovRadians, aspect, nearPlane, farPlane);
}

inline mat4 perspective_rev_z(float fovRadians, float aspect, float nearPlane)
{
    using T = float;
    T const range = tan(fovRadians / static_cast<T>(2)) * nearPlane;
    T const left = -range * aspect;
    T const right = range * aspect;
    T const bottom = -range;
    T const top = range;

    glm::mat4 Result(static_cast<T>(0));
    Result[0][0] = (static_cast<T>(2) * nearPlane) / (right - left);
    Result[1][1] = (static_cast<T>(2) * nearPlane) / (top - bottom);
    Result[2][2] = static_cast<T>(0);
    Result[2][3] = -static_cast<T>(1);
    Result[3][2] = nearPlane;
    return Result;
}

inline int ceil(float val) { return glm::ceil(val); }

inline int floor(float val) { return glm::floor(val); }

inline int abs(int val) { return glm::abs(val); }

inline float abs(float val) { return glm::abs(val); }

inline unsigned int align(unsigned int size, unsigned int alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

enum class Orientation : uint32_t
{
    IDENTITY = 0,
    ROTATE_90 = 1,
    ROTATE_270 = 2,
    ROTATE_180 = 3,
};

inline void get_screen_affine(Orientation ori, float width, float height, mat2& outTransform, vec2& outOffset)
{
    switch (ori)
    {
        case Orientation::IDENTITY:
            outTransform = mat2(2.f / width, 0, 0, 2.f / height);
            outOffset = vec2(-1.f, -1.f);
            break;
        case Orientation::ROTATE_90:
            outTransform = mat2(0.f, 2.f / width, -2.f / height, 0.f);
            outOffset = vec2(1.f, -1.f);
            break;
        case Orientation::ROTATE_270:
            outTransform = mat2(0.f, -2.f / width, 2.f / height, 0.f);
            outOffset = vec2(-1.f, 1.f);
            break;
        case Orientation::ROTATE_180:
            outTransform = mat2(0.f, -2.f / width, -2.f / height, 0.f);
            outOffset = vec2(1.f, 1.f);
            break;
    }
}

inline mat4 get_perspective_rot(Orientation ori)
{
    switch (ori)
    {
        case Orientation::IDENTITY:
            return mat4(-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        case Orientation::ROTATE_90:
            return mat4(0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        case Orientation::ROTATE_270:
            return mat4(0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        case Orientation::ROTATE_180:
            return mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    }
    return mat4(1.f);
}

inline float atan(float val) { return glm::atan(val); }
inline float tan(float val) { return glm::tan(val); }
}  // namespace OneGame::Engine::math
