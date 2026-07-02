#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::ECS
{

math::mat4 ComponentCamera::view() const { return math::lookAt(position, position + forward, glm::vec3(0, 1, 0)); }

math::vec3 ScreenToRay(ComponentCamera camera, ComponentPerspectiveCamera pcamera, math::vec2 pos)
{
    float ndcX = pos.x * 2.0f - 1.0f;
    float ndcY = 1.0f - pos.y * 2.0f;

    float tanHalfFov = tanf(pcamera.fov * 0.5f);

    float viewX = ndcX * pcamera.aspect * tanHalfFov;
    float viewY = ndcY * tanHalfFov;
    float viewZ = 1.0f;

    math::vec3 up = {0, 1, 0};
    math::vec3 right = math::normalize(math::cross(up, camera.forward));
    math::vec3 normUp = math::normalize(math::cross(camera.forward, right));
    math::vec3 rayDir = viewX * right + viewY * normUp + viewZ * camera.forward;

    rayDir = normalize(rayDir);
    return rayDir;
}

math::vec2 RayToPitchYaw(math::vec3 ray)
{
    float pitch = asinf(ray.y);
    float yaw = atan2f(ray.x, -ray.z);
    return math::vec2(yaw, pitch);
}

// math::vec3 ComponentCamera::toRay(math::vec2 input, float aspect) const
// {
//     float ndcX = input.x * 2.0f - 1.0f;
//     float ndcY = 1.0f - input.y * 2.0f;

//     float tanHalfFov = tanf(fovY * 0.5f);

//     float viewX = ndcX * aspect * tanHalfFov;
//     float viewY = ndcY * tanHalfFov;
//     float viewZ = -1.0f;

//     math::vec3 rayDir =
//     viewX * camRight +
//     viewY * camUp +
//     viewZ * camForward;

//     rayDir = normalize(rayDir);
//     return rayDir;
// }

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

void SubsystemCamera::Initialize(GameWorldContext& game, AppContext ctx)
{
    game.world.on_construct<ScreenRect>().connect<&SubsystemCamera::onViewPanelUpdate>(this);
    game.world.on_update<ScreenRect>().connect<&SubsystemCamera::onViewPanelUpdate>(this);
    game.world.on_construct<ViewPanel>().connect<&SubsystemCamera::onViewPanelUpdate>(this);
    game.world.on_update<ViewPanel>().connect<&SubsystemCamera::onViewPanelUpdate>(this);
}

void SubsystemCamera::onViewPanelUpdate(entt::registry& world, entt::entity entity)
{
    auto [vp, rect] = world.try_get<ViewPanel, ScreenRect>(entity);
    if (vp != nullptr && rect != nullptr)
    {
        auto camEntity = vp->activeCamera;
        if (auto pcam = world.try_get<ComponentPerspectiveCamera>(camEntity))
        {
            pcam->aspect = (float)rect->extent.x / (float)rect->extent.y;
        }
    }
}

void SubsystemCamera::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd) {}

void SubsystemCamera::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd)
{
    using namespace Graphics;
    for (auto [entity, view, rect] : game.world.view<ViewPanel, ScreenRect>().each())
    {
        auto e = fd.presentationWorld.create();
        fd.presentationWorld.emplace<PGameView>(e, rect.pos, rect.extent, view.activeSlot);
        if (auto camera = game.world.try_get<ComponentCamera>(view.activeCamera))
            fd.presentationWorld.emplace<PViewTransform>(e, camera->view());
        if (auto pcamera = game.world.try_get<ComponentPerspectiveCamera>(view.activeCamera))
            fd.presentationWorld.emplace<PPerspectiveTransform>(e, pcamera->fov, pcamera->aspect);
    }
}
}  // namespace OneGame::Engine::ECS
