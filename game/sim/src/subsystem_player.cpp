#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/aabb_ops.hpp"
#include "oge/math.hpp"

namespace game
{
entt::entity ComponentPlayer::CreatePlayer(oge::runtime::OgeRegistry& world,
                                           PlayerInfo info, entt::entity hint)
{
    entt::entity res;
    if (hint == entt::null)
        res = world.create();
    else if (world.valid(hint))
        res = hint;
    else
        res = world.create(hint);
    world.emplace<UpdateTag<UpdateType::Realtime>>(res);
    world.emplace<RenderStrategyTag<RenderStrategy::Interpolation>>(res);
    auto& b = world.emplace<ComponentPhysicBody>(res, info.latestPosition);
    b.stepAssist = 1.01f;
    world.emplace<ComponentAABBCollider>(
        res, ComponentAABBCollider{.aabb = {math::vec3{0.f, 0.f, 0.f},
                                            math::vec3{0.7f, 1.8f, 0.7f}}});
    world.emplace<ComponentCamera>(res, math::vec3{20.f, 20.f, 20.f});
    world.emplace<ComponentPerspectiveCamera>(res);
    world.emplace<input::PlayerInputStream>(res);
    auto& c = world.emplace<ComponentCreature>(
        res, ComponentCreature{.maxSpeed = 4.f});
    c.SetMaxJumpHeight(1.65f);
    world.emplace<ComponentPlayer>(res, info.uuid);
    return res;
}

void ComponentPlayer::DestroyPlayer(oge::runtime::OgeRegistry& world,
                                    PlayerInfo info)
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
        if constexpr (variant == UpdateType::Realtime)
        {
            auto& cursor = player.inputCursor;
            input.AdvanceTick();

            // Drain every pending frame — the client may produce input at a
            // different rate than this world ticks.  Consuming only one
            // frame per tick silently drops the backlog once the 16-slot
            // ring wraps.  moveOrder is a per-tick order (magnitude <= 1),
            // so the last frame's move wins rather than summing.
            math::vec3 finalMove{};
            bool consumed = false;
            uint32_t cnt = 0;
            {
                input::PlayerInputFrame frame;
                while (input.PollFrame(cursor, frame))
                {
                    consumed = true;
                    ++cnt;

                    if (frame.hasAim)
                    {
                        camera.SetYawPitch(frame.aim.x, frame.aim.y);
                    }

                    finalMove += frame.move;
                }
            }

            finalMove /= (float)cnt;

            if (!consumed) continue;

            if (body.enableGravity)
            {
                creature.moveOrder = finalMove;
            }
            else
            {
                auto moveLen = math::len(finalMove);
                auto r = math::sqrt((finalMove.x * finalMove.x) +
                                    (finalMove.z * finalMove.z));
                creature.moveOrder = moveLen > input::INPUT_EPSILON
                                         ? finalMove * r / moveLen
                                         : math::vec3{};
            }

            camera.position =
                body.pos +
                math::vec3{(collider.aabb.min.x + collider.aabb.max.x) / 2.f,
                           1.65f,
                           (collider.aabb.min.z + collider.aabb.max.z) / 2.f} -
                camera.forward * 3.f;
            // ctx.world.patch<ComponentCamera>(entity);

            // Continuous block targeting for highlight rendering
            auto raycastResult =
                terrain.CastRay(camera.position, camera.forward);
            if (raycastResult.has_value())
                ctx.world.emplace_or_replace<ComponentTargetBlock>(
                    entity, raycastResult->hitPos, true);
            else
                ctx.world.emplace_or_replace<ComponentTargetBlock>(
                    entity, Point3{}, false);
        }

        if constexpr (variant == UpdateType::FixedStep)
        {
            auto& cursor = player.actionCursor;
            input::PlayerInputFrame frame;
            creature.jumpOrder = false;
            while (input.PollFrame(cursor, frame))
            {
                size_t actionIdx = 0;
                while (actionIdx < frame.inputEventCnt)
                {
                    auto event = frame.inputEvents[actionIdx];
                    actionIdx++;
                    creature.jumpOrder =
                        creature.jumpOrder || event.get<PlayerAction::Jump>();
                    event.unset<PlayerAction::Jump>();
                    if (event.actionMask != 0)
                    {
                        if (player.lastActionTime <= 0.f)
                        {
                            auto raycastResult = terrain.CastRay(
                                camera.position,
                                ViewToRay(camera, event.actionPos));
                            if (raycastResult.has_value())
                            {
                                if (event.get<PlayerAction::Digging>())
                                {
                                    terrain.SetBlock(
                                        raycastResult.value().hitPos, 0);
                                }
                                if (event.get<PlayerAction::Placing>())
                                {
                                    auto blockId = blocks.GetBlockId("stone");
                                    auto blockValue = blockId;
                                    auto placePos =
                                        raycastResult->hitPos +
                                        oge::perFaceOffset[raycastResult
                                                               ->hitFace];
                                    auto blkAABBs =
                                        blocks.GetBlockAABBList(blockId);
                                    bool canPlace = true;
                                    for (auto blkAABB : blkAABBs)
                                    {
                                        if (CheckOverlap(
                                                collider.aabb + body.pos,
                                                blkAABB + placePos))
                                        {
                                            canPlace = false;
                                            break;
                                        }
                                    }
                                    if (canPlace)
                                        terrain.SetBlock(placePos, blockValue);
                                }
                                player.lastActionTime = 0.3f;
                            }
                        }
                    }
                    else
                    {
                        player.lastActionTime = 0.f;
                    }
                }
            }

            player.lastActionTime -= ctx.dt;
        }
    }
}

DECL_UTYPES_IMPL(SubsystemPlayer)
}  // namespace sim
}  // namespace game
