#include <iterator>

#include "game/components.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/aabb_ops.hpp"
#include "oge/fmt.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"

namespace game
{
entt::entity ComponentPlayer::CreatePlayer(entt::registry& world, math::vec3 pos)
{
    auto res = world.create();
    world.emplace<UpdateTag<UpdateType::Realtime>>(res);
    auto& b = world.emplace<ComponentPhysicBody>(res, pos);
    b.stepAssist = 1.01f;
    world.emplace<ComponentAABBCollider>(
        res, ComponentAABBCollider{.aabb = {math::vec3{0.f, 0.f, 0.f}, math::vec3{0.7f, 1.8f, 0.7f}}});
    world.emplace<ComponentCamera>(res);
    world.emplace<ComponentPerspectiveCamera>(res);
    world.emplace<input::PlayerInputStream>(res);
    auto& c = world.emplace<ComponentCreature>(res, ComponentCreature{.maxSpeed = 4.f});
    c.SetMaxJumpHeight(1.65f);
    world.emplace<ComponentPlayer>(res);
    return res;
}

namespace sim
{
using ::game::input::PlayerAction;
using ::game::input::PlayerInputEvent;
using ::game::input::PlayerInputStream;
using ::game::terrain::BlockRegistry;
using ::game::terrain::TerrainView;

template <UpdateType variant>
void SubsystemPlayer<variant>::onAttach(GameState& ctx)
{
}

template <UpdateType variant>
void SubsystemPlayer<variant>::onDetach(GameState& ctx)
{
}

template <UpdateType variant>
void SubsystemPlayer<variant>::onUpdate(FGameState& ctx)
{
    auto& terrain = ctx.world.ctx().get<TerrainView>();
    auto& blocks = ctx.world.ctx().get<BlockRegistry>();
    for (auto [entity, camera, pcam, input, collider, player, body, creature] :
         ctx.world
             .view<ComponentCamera, const ComponentPerspectiveCamera, PlayerInputStream, const ComponentAABBCollider,
                   ComponentPlayer, ComponentPhysicBody, ComponentCreature>()
             .each())
    {
        if constexpr (variant == UpdateType::Realtime)
        {
            math::vec2 panDelta;
            if (input.PollPanDelta(panDelta))
            {
                camera.ApplyDelta(panDelta.x, panDelta.y);
            }

            math::vec2 moveDelta;
            if (input.PollMoveDelta(moveDelta))
            {
                auto right = camera.right();
                if (body.enableGravity)
                {
                    creature.moveOrder = math::normalize({camera.forward.x, 0, camera.forward.z}) * moveDelta.y +
                                         math::vec3{right.x, 0, right.z} * moveDelta.x;
                }
                else
                {
                    creature.moveOrder = camera.forward * moveDelta.y + right * moveDelta.x;
                }
            }

            camera.position = body.pos + math::vec3{(collider.aabb.min.x + collider.aabb.max.x) / 2.f, 1.65f,
                                                    (collider.aabb.min.z + collider.aabb.max.z) / 2.f};
        }

        if constexpr (variant == UpdateType::FixedStep)
        {
            PlayerInputEvent event;
            creature.jumpOrder = false;
            while (input.PollAction(event))
            {
                creature.jumpOrder = creature.jumpOrder || event.get<PlayerAction::Jump>();
                if (event.actionMask != 0 && player.lastActionTime <= 0.f)
                {
                    auto raycastResult = terrain.CastRay(camera.position, ScreenToRay(camera, pcam, event.actionPos));
                    if (raycastResult.has_value())
                    {
                        if (event.get<PlayerAction::Digging>())
                        {
                            terrain.SetBlock(raycastResult.value().hitPos, 0);
                        }
                        if (event.get<PlayerAction::Placing>())
                        {
                            auto blockId = blocks.GetBlockId("stone");
                            auto blockValue = blockId;
                            auto placePos = raycastResult->hitPos + oge::perFaceOffset[raycastResult->hitFace];
                            auto blkAABBs = blocks.GetBlockAABBList(blockId);
                            bool canPlace = true;
                            for (auto blkAABB : blkAABBs)
                            {
                                if (CheckOverlap(collider.aabb + body.pos, blkAABB + placePos))
                                {
                                    canPlace = false;
                                    break;
                                }
                            }
                            if (canPlace) terrain.SetBlock(placePos, blockValue);
                        }
                        player.lastActionTime = 1.2f;
                    }
                }
                else
                {
                    LOG_DEBUG("reset action time");
                    player.lastActionTime = 0.f;
                }
            }

            player.lastActionTime -= ctx.dt;
        }
    }
}

DECL_UTYPES_IMPL(SubsystemPlayer)
}  // namespace sim
}  // namespace game
