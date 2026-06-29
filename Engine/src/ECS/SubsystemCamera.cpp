#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::ECS
{
struct ComponentCamera
{
    float yaw;
    float pitch;
    math::vec3 position;
    glm::vec3 forward;

    entt::entity playerInputData;

    math::mat4 view() const
    {
        return math::lookAt(position, position + forward, glm::vec3(0, 1, 0));
        ;
    }

    void ApplyDelta(float dsx, float dsy, float dwx, float dwz)
    {
        yaw += dsx;
        pitch += dsy;
        pitch = math::clamp(pitch, -math::radians(89.0f), math::radians(89.0f));

        forward.x = cos(pitch) * sin(yaw);
        forward.y = sin(pitch);
        forward.z = cos(pitch) * cos(yaw);
        forward = math::normalize(forward);

        glm::vec3 worldUp = {0, 1, 0};

        // Horizontal movement direction
        // glm::vec3 flatForward = forward;
        // flatForward.y = 0.0f;
        // flatForward = glm::normalize(flatForward);

        // glm::vec3 right = glm::normalize(glm::cross(flatForward, worldUp));

        // position += dwx * right + dwz * flatForward;

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

        position += dwx * math::cross(forward, glm::vec3(0, 1, 0)) + dwz * forward;
    }
};

void SubsystemCamera::Initialize(GameWorldContext& game, AppContext ctx)
{
    auto camera = game.world.create();
    ComponentCamera& cam = game.world.emplace<ComponentCamera>(camera);

    cam.position = {20.f, 20.f, 20.f};

    glm::vec3 target = {0.f, 0.f, 0.f};
    cam.forward = glm::normalize(target - cam.position);

    cam.yaw = std::atan2(cam.forward.x, cam.forward.z);
    cam.pitch = std::asin(cam.forward.y);
    cam.playerInputData = game.world.view<PlayerInputData>().front();
}

void SubsystemCamera::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    float dt = fd.dt;
    auto cameraEntity = game.world.view<ComponentCamera>().front();
    auto& camera = game.world.get<ComponentCamera>(cameraEntity);

    auto& data = game.world.get<const PlayerInputData>(camera.playerInputData);

    camera.ApplyDelta(data.panDelta.x, data.panDelta.y, data.moveDelta.x, data.moveDelta.y);
}

void SubsystemCamera::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd)
{
    auto e = fd.presentationWorld.create();
    auto& view = fd.presentationWorld.emplace<Graphics::PViewTransform>(e);

    auto cameraEntity = game.world.view<ComponentCamera>().front();
    auto& camera = game.world.get<ComponentCamera>(cameraEntity);

    view.view = camera.view();
}
}  // namespace OneGame::Engine::ECS
