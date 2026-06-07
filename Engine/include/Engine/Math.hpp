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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi


namespace OneGame::Engine::math
{
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;

    using mat4 = glm::mat4;
    using quat = glm::quat;

    const float pi = glm::pi<float>();

    inline float radians(float degrees)
    {
        return glm::radians(degrees);
    }

    inline float degrees(float radians)
    {
        return glm::degrees(radians);
    }

    inline mat4 identity()
    {
        return mat4(1.0f);
    }

    inline mat4 translate(const mat4& m, const vec3& v)
    {
        return glm::translate(m, v);
    }

    inline mat4 rotate(const mat4& m, float angleRadians, const vec3& axis)
    {
        return glm::rotate(m, angleRadians, axis);
    }

    inline mat4 scale(const mat4& m, const vec3& v)
    {
        return glm::scale(m, v);
    }

    inline mat4 translate(const vec3& v)
    {
        return translate(identity(), v);
    }

    inline mat4 rotate(float angleRadians, const vec3& axis)
    {
        return rotate(identity(), angleRadians, axis);
    }

    inline mat4 scale(const vec3& v)
    {
        return scale(identity(), v);
    }

    inline mat4 lookAt(
        const vec3& eye,
        const vec3& center,
        const vec3& up)
    {
        return glm::lookAt(eye, center, up);
    }

    inline mat4 perspective(
        float fovRadians,
        float aspect,
        float nearPlane,
        float farPlane)
    {
        return glm::perspective(
            fovRadians,
            aspect,
            nearPlane,
            farPlane);
    }

    inline unsigned int align(unsigned int size, unsigned int alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }
}
