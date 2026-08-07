#include <iterator>

#include "game/components.hpp"
#include "game/game_world.hpp"
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
entt::entity ComponentPlayer::CreatePlayer(entt::registry& world,
                                           PlayerInfo info, entt::entity hint)
{
    entt::entity res;
    if (hint == entt::null)
        res = world.create();
    else
        res = world.create(hint);
    world.emplace<UpdateTag<UpdateType::Realtime>>(res);
    auto& b = world.emplace<ComponentPhysicBody>(res, info.latestPosition);
    b.stepAssist = 1.01f;
    world.emplace<ComponentAABBCollider>(
        res, ComponentAABBCollider{.aabb = {math::vec3{0.f, 0.f, 0.f},
                                            math::vec3{0.7f, 1.8f, 0.7f}}});
    world.emplace<ComponentCamera>(
        res, math::vec3{20.f, 20.f, 20.f});
    world.emplace<ComponentPerspectiveCamera>(res);
    world.emplace<input::PlayerInputStream>(res);
    auto& c = world.emplace<ComponentCreature>(
        res, ComponentCreature{.maxSpeed = 4.f});
    c.SetMaxJumpHeight(1.65f);
    world.emplace<ComponentPlayer>(res, info.uuid);
    return res;
}

void ComponentPlayer::DestroyPlayer(entt::registry& world, PlayerInfo info)
{
    for (auto [e, player] : world.view<ComponentPlayer>()->each())
    {
        if (player.id == info.uuid)
        {
            world.destroy(e);
        }
    }
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
             .view<ComponentCamera, const ComponentPerspectiveCamera,
                   PlayerInputStream, const ComponentAABBCollider,
                   ComponentPlayer, ComponentPhysicBody, ComponentCreature>()
             .each())
    {
        auto& cursor = player.inputCursor;
        if constexpr (variant == UpdateType::Realtime)
        {
            input.AdvanceTick();
            math::vec2 panDelta;
            if (input.PollAccumPan(cursor, panDelta))
            {
                camera.SetYawPitch(panDelta.x, panDelta.y);
            }

            math::vec2 moveDelta;
            if (input.PollMoveDelta(cursor, moveDelta))
            {
                auto right = camera.right();
                if (body.enableGravity)
                {
                    creature.moveOrder =
                        math::normalize(
                            {camera.forward.x, 0, camera.forward.z}) *
                            moveDelta.y +
                        math::normalize(math::vec3{right.x, 0, right.z}) *
                            moveDelta.x;
                }
                else
                {
                    creature.moveOrder =
                        camera.forward * moveDelta.y + right * moveDelta.x;
                }
                if (math::len_sq(creature.moveOrder) >= 1.0f)
                    creature.moveOrder = math::normalize(creature.moveOrder);
            }

            camera.position =
                body.pos +
                math::vec3{(collider.aabb.min.x + collider.aabb.max.x) / 2.f,
                           1.65f,
                           (collider.aabb.min.z + collider.aabb.max.z) / 2.f};
            ctx.world.patch<ComponentCamera>(entity);
        }

        if constexpr (variant == UpdateType::FixedStep)
        {
            PlayerInputEvent event;
            creature.jumpOrder = false;
            while (input.PollAction(cursor, event))
            {
                creature.jumpOrder =
                    creature.jumpOrder || event.get<PlayerAction::Jump>();
                if (event.actionMask != 0)
                {
                    if (player.lastActionTime <= 0.f)
                    {
                        auto raycastResult =
                            terrain.CastRay(camera.position,
                                            ViewToRay(camera, event.actionPos));
                        if (raycastResult.has_value())
                        {
                            if (event.get<PlayerAction::Digging>())
                            {
                                terrain.SetBlock(raycastResult.value().hitPos,
                                                 0);
                            }
                            if (event.get<PlayerAction::Placing>())
                            {
                                auto blockId = blocks.GetBlockId("stone");
                                auto blockValue = blockId;
                                auto placePos =
                                    raycastResult->hitPos +
                                    oge::perFaceOffset[raycastResult->hitFace];
                                auto blkAABBs =
                                    blocks.GetBlockAABBList(blockId);
                                bool canPlace = true;
                                for (auto blkAABB : blkAABBs)
                                {
                                    if (CheckOverlap(collider.aabb + body.pos,
                                                     blkAABB + placePos))
                                    {
                                        canPlace = false;
                                        break;
                                    }
                                }
                                if (canPlace)
                                    terrain.SetBlock(placePos, blockValue);
                            }
                            player.lastActionTime = 0.01f;
                        }
                    }
                }
                else
                {
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
