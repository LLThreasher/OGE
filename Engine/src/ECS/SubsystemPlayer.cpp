#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Point3.hpp"
#include "Engine/Formaters.hpp"


namespace OneGame::Engine::ECS
{
void SubsystemPlayer::Initialize(GameWorldContext& game, AppContext ctx)
{
}

void SubsystemPlayer::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    auto& terrain = game.ctx().get<Terrain::TerrainView>();
    auto& blocks = game.ctx().get<Terrain::BlockRegistry>();
    for (auto [entity, camera, pcam, input, player] : game.view<ComponentCamera, const ComponentPerspectiveCamera, const PlayerInputData, ComponentPlayer>().each())
    {
        camera.ApplyDelta(input.panDelta.x, input.panDelta.y, input.moveDelta.x, input.moveDelta.y);
        // player.lookingAt = game.terrain.CastRay(camera.position, camera.forward);
        if (input.get<PlayerAction::Digging>())
        {
            auto raycastResult = terrain.CastRay(camera.position, ScreenToRay(camera, pcam, input.actionPos));
            if (raycastResult.has_value())
                terrain.SetBlock(raycastResult.value().hitPos, 0);
        }
        if (input.get<PlayerAction::Placing>())
        {
            auto raycastResult = terrain.CastRay(camera.position, ScreenToRay(camera, pcam, input.actionPos));
            if (raycastResult.has_value())
            {
                auto blockId = blocks.GetBlockId("stone");
                auto blockValue = blockId;
                terrain.SetBlock(raycastResult->hitPos + perFaceOffset[raycastResult->hitFace], blockValue);
            }
        }
    }
}

void SubsystemPlayer::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd)
{
    // auto players = game.view<ComponentPlayer>();
    // if (!players.empty())
    // {
    //     auto player = game.get<ComponentPlayer>(players.front());
    //     if (player.lookingAt.has_value())
    //         AddDebugInfo(
    //             fd.presentationWorld,
    //             std::format("Looking at {} {} {}[{}]", player.lookingAt->hitPos, player.lookingAt->hitFace,
    //                         game.blocks.GetBlockDisplayName(game.blocks.GetBlockId(player.lookingAt->hitBlockValue)),
    //                         player.lookingAt->hitBlockValue));
    // }
}
}  // namespace OneGame::Engine::ECS