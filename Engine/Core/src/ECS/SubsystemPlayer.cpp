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
}  // namespace OneGame::Engine::ECS