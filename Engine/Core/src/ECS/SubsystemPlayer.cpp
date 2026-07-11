#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Formaters.hpp"
#include "Engine/Point3.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainView.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemPlayer::Initialize(GameWorldContext& game, AppContext ctx) {}

void SubsystemPlayer::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    auto& terrain = game.ctx().get<Terrain::TerrainView>();
    auto& blocks = game.ctx().get<Terrain::BlockRegistry>();
    for (auto [entity, camera, pcam, input, collider, player, body] :
         game.view<ComponentCamera, const ComponentPerspectiveCamera, const PlayerInputData,
                   const ComponentAABBCollider, ComponentPlayer, ComponentPhysicBody>()
             .each())
    {
        camera.ApplyDelta(input.panDelta.x, input.panDelta.y, 0.f, 0.f);
        camera.position = body.pos + math::vec3{(collider.aabb.min.x + collider.aabb.max.x) / 2.f, 1.75f,
                                                (collider.aabb.min.z + collider.aabb.max.z) / 2.f};
        body.velocity += (math::normalize({camera.forward.x, 0.f, camera.forward.z}) * input.moveDelta.y +
                            camera.right() * input.moveDelta.x) * 0.5f;

        // // add small amount of velocity
        // body.velocity += body.acceleration * fd.dt * 10.f;
        // body.acceleration *= 5.f;
        // player.lookingAt = game.terrain.CastRay(camera.position, camera.forward);
        if (!input.empty())
        {
            if (input.get<PlayerAction::Jump>() && body.isGrounded) body.velocity.y += 5.f;
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