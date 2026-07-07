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
        if (!input.empty())
        {
            if (player.lastActionTime <= 0.0f)
            {
                auto raycastResult = terrain.CastRay(camera.position, ScreenToRay(camera, pcam, input.actionPos));
                if (raycastResult.has_value())
                {
                    if (input.get<PlayerAction::Digging>())
                    {
                        terrain.SetBlock(raycastResult.value().hitPos, 0);
                    }
                    if (input.get<PlayerAction::Placing>())
                    {
                        auto blockId = blocks.GetBlockId("stone");
                        auto blockValue = blockId;
                        terrain.SetBlock(raycastResult->hitPos + perFaceOffset[raycastResult->hitFace], blockValue);
                    }
                    player.lastActionTime = 0.2f;
                }
            }
            else
            {
                player.lastActionTime -= fd.dt;
            }
        }
        else
        {
            player.lastActionTime = 0.f;
        }
    }
}
}  // namespace OneGame::Engine::ECS