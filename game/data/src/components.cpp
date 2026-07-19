#include "game/components.hpp"

namespace game {

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

void ComponentCamera::ApplyDelta(float dsx, float dsy, float dwx, float dwz)
{
    yaw += dsx;
    pitch += dsy;
    pitch = math::clamp(pitch, -math::radians(89.0f), math::radians(89.0f));

    forward.x = math::cos(pitch) * math::sin(yaw);
    forward.y = math::sin(pitch);
    forward.z = math::cos(pitch) * math::cos(yaw);
    forward = math::normalize(forward);

    position += dwx * right() + dwz * forward;
}

math::mat4 ComponentCamera::view() const { return math::lookAt(position, position + forward, glm::vec3(0, 1, 0)); }

}