#include "Engine/AABBOps.hpp"
#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Formaters.hpp"
#include "Engine/Point3.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainView.hpp"

namespace OneGame::Engine::ECS
{

entt::entity ComponentPlayer::CreatePlayer(entt::registry& world, math::vec3 pos)
{
    auto res = world.create();
    world.emplace<ComponentPhysicBody>(res, pos);
    world.emplace<ComponentAABBCollider>(
        res, ComponentAABBCollider{.aabb = {math::vec3{0.f, 0.f, 0.f}, math::vec3{0.7f, 1.8f, 0.7f}}});
    world.emplace<ComponentCamera>(res);
    world.emplace<ComponentPerspectiveCamera>(res);
    world.emplace<ComponentCreature>(res, ComponentCreature{.maxSpeed = 4.f, .maxJumpHeight = 1.0f});
    world.emplace<ComponentPlayer>(res);
    world.emplace<PlayerInputData>(res);
    return res;
}

void SubsystemPlayer::Initialize(GameWorldContext& game, AppContext ctx) {}

void SubsystemPlayer::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    auto& terrain = game.ctx().get<Terrain::TerrainView>();
    auto& blocks = game.ctx().get<Terrain::BlockRegistry>();
    for (auto [entity, camera, pcam, input, collider, player, body, creature] :
         game.view<ComponentCamera, const ComponentPerspectiveCamera, const PlayerInputData,
                   const ComponentAABBCollider, ComponentPlayer, ComponentPhysicBody, ComponentCreature>()
             .each())
    {
        camera.ApplyDelta(input.panDelta.x, input.panDelta.y, 0.f, 0.f);
        camera.position = body.pos + math::vec3{(collider.aabb.min.x + collider.aabb.max.x) / 2.f, 1.75f,
                                                (collider.aabb.min.z + collider.aabb.max.z) / 2.f};
        auto right = camera.right();
        if (body.enableGravity)
        {
            creature.moveOrder = math::normalize({camera.forward.x, 0, camera.forward.z}) * input.moveDelta.y +
                                 math::vec3{right.x, 0, right.z} * input.moveDelta.x;
        }
        else
        {
            creature.moveOrder = camera.forward * input.moveDelta.y + right * input.moveDelta.x;
        }

        creature.jumpOrder = input.get<PlayerAction::Jump>() && body.isGrounded;
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
                        auto placePos = raycastResult->hitPos + perFaceOffset[raycastResult->hitFace];
                        auto blkAABBs = blocks.GetBlockAABBList(blockId);
                        bool canPlace = true;
                        CollisionResult res;
                        for (auto blkAABB : blkAABBs)
                        {
                            if (CheckCollision(collider.aabb + body.pos, blkAABB + placePos, res))
                            {
                                canPlace = false;
                                break;
                            }
                        }
                        if (canPlace) terrain.SetBlock(placePos, blockValue);
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