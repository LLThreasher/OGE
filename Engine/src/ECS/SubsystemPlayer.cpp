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
    for (auto [entity, camera, pcam, input, player] : game.world.view<ComponentCamera, const ComponentPerspectiveCamera, const PlayerInputData, ComponentPlayer>().each())
    {
        camera.ApplyDelta(input.panDelta.x, input.panDelta.y, input.moveDelta.x, input.moveDelta.y);
        // player.lookingAt = game.terrain.CastRay(camera.position, camera.forward);
        if (input.digging)
        {
            auto raycastResult = game.terrain.CastRay(camera.position, ScreenToRay(camera, pcam, input.diggingPos));
            if (raycastResult.has_value())
                game.terrain.SetBlock(raycastResult.value().hitPos, 0);
        }
        if (input.placing)
        {
            auto raycastResult = game.terrain.CastRay(camera.position, ScreenToRay(camera, pcam, input.placingPos));
            if (raycastResult.has_value())
            {
                auto blockId = game.blocks.GetBlockId("stone");
                auto blockValue = blockId;
                game.terrain.SetBlock(raycastResult->hitPos + perFaceOffset[raycastResult->hitFace], blockValue);
            }
        }
    }
}

void SubsystemPlayer::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd)
{
    // auto players = game.world.view<ComponentPlayer>();
    // if (!players.empty())
    // {
    //     auto player = game.world.get<ComponentPlayer>(players.front());
    //     if (player.lookingAt.has_value())
    //         AddDebugInfo(
    //             fd.presentationWorld,
    //             std::format("Looking at {} {} {}[{}]", player.lookingAt->hitPos, player.lookingAt->hitFace,
    //                         game.blocks.GetBlockDisplayName(game.blocks.GetBlockId(player.lookingAt->hitBlockValue)),
    //                         player.lookingAt->hitBlockValue));
    // }
}
}  // namespace OneGame::Engine::ECS