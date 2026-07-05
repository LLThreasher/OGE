
#include "Engine/ECS/Components.hpp"
#include "Engine/Math.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::ECS
{

math::mat4 ComponentCamera::view() const { return math::lookAt(position, position + forward, glm::vec3(0, 1, 0)); }

math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos)
{
    float ndcX = -pos.x * 2.0f + 1.0f;
    float ndcY = 1.0f - pos.y * 2.0f;

    float tanHalfFov = tanf(pcamera.fov * 0.5f);

    float viewX = ndcX * pcamera.aspect * tanHalfFov;
    float viewY = ndcY * tanHalfFov;
    float viewZ = 1.0f;

    math::vec3 up = camera.up();
    math::vec3 right = camera.right();
    math::vec3 rayDir = viewX * right + viewY * up + viewZ * camera.forward;

    rayDir = normalize(rayDir);
    return rayDir;
}

math::vec2 RayToPitchYaw(math::vec3 ray)
{
    float pitch = asinf(ray.y);
    float yaw = atan2f(ray.x, -ray.z);
    return math::vec2(yaw, pitch);
}

void ComponentCamera::ApplyDelta(float dsx, float dsy, float dwx, float dwz)
{
    yaw += dsx;
    pitch += dsy;
    pitch = math::clamp(pitch, -math::radians(89.0f), math::radians(89.0f));

    forward.x = cos(pitch) * sin(yaw);
    forward.y = sin(pitch);
    forward.z = cos(pitch) * cos(yaw);
    forward = math::normalize(forward);

    glm::vec3 worldUp = {0, 1, 0};

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    position += dwx * math::normalize(math::cross(forward, glm::vec3(0, 1, 0))) + dwz * forward;
}

}  // namespace OneGame::Engine::ECS
