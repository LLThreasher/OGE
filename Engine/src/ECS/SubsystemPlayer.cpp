#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include <format>


namespace OneGame::Engine::ECS
{
void SubsystemPlayer::Initialize(GameWorldContext& game, AppContext ctx)
{
    auto view = game.world.view<PlayerViewPanel>().front();
    
    ComponentCamera& cam = game.world.emplace<ComponentCamera>(view);

    cam.position = {20.f, 20.f, 20.f};

    glm::vec3 target = {0.f, 0.f, 0.f};
    cam.forward = glm::normalize(target - cam.position);

    cam.yaw = std::atan2(cam.forward.x, cam.forward.z);
    cam.pitch = std::asin(cam.forward.y);

    ComponentPerspectiveCamera& pcam = game.world.emplace<ComponentPerspectiveCamera>(view);
    pcam.fov = math::radians(45.f);
    auto rect = game.world.get<UIRect>(view);
    pcam.aspect = rect.extent.x / rect.extent.y;

    auto player = ComponentPlayer::CreatePlayer(game.world, view);
}

void SubsystemPlayer::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    for (auto [entity, player] : game.world.view<ComponentPlayer>().each())
    {
        auto& input = game.world.get<const PlayerInputData>(player.playerInputEntity);
        auto& camera = game.world.get<ComponentCamera>(player.playerInputEntity);
        camera.ApplyDelta(input.panDelta.x, input.panDelta.y, input.moveDelta.x, input.moveDelta.y);
        player.lookingAt = game.terrain.CastRay(camera.position, camera.forward);
    }
}

void SubsystemPlayer::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd)
{
    auto players = game.world.view<ComponentPlayer>();
    if (!players.empty())
    {
        auto player = game.world.get<ComponentPlayer>(players.front());
        if (player.lookingAt.has_value())
            AddDebugInfo(fd.presentationWorld, std::format("Looking at {} {} {}[{}]", player.lookingAt->hitPos, player.lookingAt->hitFace, game.blocks.GetBlockDisplayName(game.blocks.GetBlockId(player.lookingAt->hitBlockValue)), player.lookingAt->hitBlockValue));
    }
}
}  // namespace OneGame::Engine::ECS