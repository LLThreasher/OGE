#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/sim/player_sim_config.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/aabb_ops.hpp"
#include "oge/log.hpp"
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
    // The fixed pipeline simulates every player (like the server) — the tag
    // also replicates so the client's authoritative world drives the parity
    // sim.
    world.emplace<UpdateTag<UpdateType::FixedStep>>(res);
    world.emplace<RenderStrategyTag<RenderStrategy::Interpolation>>(res);
    auto& b = world.emplace<ComponentPhysicBody>(res, info.latestPosition);
    b.stepAssist = 1.01f;
    world.emplace<ComponentAABBCollider>(
        res, ComponentAABBCollider{.aabb = {math::vec3{0.f, 0.f, 0.f},
                                            math::vec3{0.7f, 1.8f, 0.7f}}});
    world.emplace<ComponentCamera>(res, math::vec3{20.f, 20.f, 20.f});
    world.emplace<ComponentPerspectiveCamera>(res);
    world.emplace<input::PlayerInputStream>(res);
    world.emplace<input::PlayerActionStream>(res);
    // Fixed-tick input read state (D3): non-replicated per-entity cursors +
    // the cached current-tick frame.
    world.emplace<input::PlayerSimInputState>(res);
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
// Max accepted distance between the server's body and the client-stamped
// jump lift-off position.  Latency drift at run speed is a few tens of cm;
// anything beyond this is desync or a fabricated stamp.
constexpr float kMaxJumpStampDelta = 1.0f;

using ::game::input::PlayerActionKind;
using ::game::input::PlayerInputStream;
using ::game::terrain::BlockRegistry;
using ::game::terrain::TerrainView;

// Apply a ray-encoded dig/place action (D6): the fixed stage casts the
// exact ray the client emitted, so the server needs no camera
// reconstruction.  Gated by the action cooldown like the pre-ray code; the
// cooldown is enforced (not reset by empty frames) so back-to-back actions
// respect the 0.3 s gap.
inline void ApplyRayAction(TerrainView& terrain, BlockRegistry& blocks,
                           const ComponentAABBCollider& collider,
                           ComponentPhysicBody& body, ComponentPlayer& player,
                           const input::PlayerAction& action)
{
    if (player.lastActionTime > 0.f)
    {
        return;
    }

    auto raycastResult = terrain.CastRay(action.origin, action.dir);
    if (!raycastResult.has_value())
    {
        return;
    }

    if (action.actionMask & (1 << static_cast<uint32_t>(PlayerActionKind::Digging)))
    {
        terrain.SetBlock(raycastResult.value().hitPos, 0);
    }
    if (action.actionMask & (1 << static_cast<uint32_t>(PlayerActionKind::Placing)))
    {
        auto blockId = blocks.GetBlockId("stone");
        auto placePos = raycastResult->hitPos +
                        oge::perFaceOffset[raycastResult->hitFace];
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
        if (canPlace)
            terrain.SetBlock(placePos, blockId);
    }

    player.lastActionTime = 0.3f;
}

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
    // UpdateTag<variant> for uniformity with creature/physics: both tags
    // now exist on every player entity (CreatePlayer / DebugVoxelView), and
    // the FixedStep tag replicates so the authoritative mirror drives the
    // parity sim.
    for (auto [entity, camera, pcam, input, collider, player, body, creature] :
         ctx.world
             .view<const UpdateTag<variant>, ComponentCamera,
                   const ComponentPerspectiveCamera, PlayerInputStream,
                   const ComponentAABBCollider, ComponentPlayer,
                   ComponentPhysicBody, ComponentCreature>()
             .each())
    {
        if constexpr (variant == UpdateType::Realtime)
        {
            auto& simState = ctx.world.get<input::PlayerSimInputState>(entity);
            input.AdvanceTick();

            // Drain every pending raw frame — the client may produce input
            // at a different rate than this world ticks.  PollEvents is
            // non-consuming (D3): frames stay in the window for the per-tick
            // aggregation.  moveOrder is a per-tick order (magnitude <= 1),
            // so the last frame's move wins rather than summing.
            math::vec3 finalMove{};
            bool consumed = false;
            uint32_t cnt = 0;
            // Canonical eye offset over the body's footprint (the chase
            // target).  Constant per entity per update.
            const math::vec3 eyeOffset{
                (collider.aabb.min.x + collider.aabb.max.x) / 2.f, 1.65f,
                (collider.aabb.min.z + collider.aabb.max.z) / 2.f};
            {
                input::PlayerInputRawFrame frame;
                while (input.PollEvents(simState.realtimeCursor, frame))
                {
                    consumed = true;
                    ++cnt;

                    if (frame.hasAim)
                    {
                        camera.SetYawPitch(frame.aim.x, frame.aim.y);
                    }

                    // Emit ray-encoded dig/place actions from this frame's
                    // events (D6): the ray is baked from the live camera
                    // with the aim already applied above, so the delivered
                    // ray is the exact aim the player had.  Chase the
                    // camera to the body first — the first action of a
                    // session would otherwise bake from the un-chased spawn
                    // camera (a session's earlier frames may not have
                    // consumed input, which is what gates the final chase
                    // below).  Jump stays in the window for the movement
                    // aggregation.  No SetBlock here — terrain edits are
                    // fixed-pipeline-only.
                    camera.position = body.pos + eyeOffset -
                                      camera.forward * 3.f;
                    for (size_t k = 0; k < frame.inputEventCnt; ++k)
                    {
                        auto event = frame.inputEvents[k];
                        const uint8_t mask =
                            event.actionMask &
                            ~(1 << static_cast<uint32_t>(PlayerActionKind::Jump));
                        if (mask == 0)
                        {
                            continue;
                        }
                        auto& actions =
                            ctx.world.get<input::PlayerActionStream>(entity);
                        actions.PushAction(input::PlayerAction{
                            mask, camera.position,
                            ViewToRay(camera, event.actionPos)});
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
                body.pos + eyeOffset - camera.forward * 3.f;
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
            // The 20 tps parity path (D1/D9): the server world and the
            // client's authoritative world run this identical stage over
            // the same SimTickContext.
            auto& simState = ctx.world.get<input::PlayerSimInputState>(entity);
            const auto& tickCtx = ctx.world.ctx().get<sim::SimTickContext>();

            // Decisions once per tick, on sub-step 0 (D4).
            if (tickCtx.subStepIdx == 0 &&
                simState.consumedTick != tickCtx.currentTick)
            {
                simState.consumedTick = tickCtx.currentTick;

                // Movement frame — one-tick pipeline delay (D3): a frame
                // produced during tick T applies during tick T+1 on both
                // sides.
                input::PlayerInputFrame frame{};
                if (input::TryReadTickFrame(input, simState.moveCursor,
                                            tickCtx.currentTick,
                                            simState.lastAppliedTick, frame))
                {
                    simState.frame = frame;
                    simState.hasFrame = true;

                    // Landing resets the stamp gate: a fresh jump press may
                    // stamp again.  Repeat stamps while the stamped arc is
                    // still airborne must not re-anchor it (the held jump
                    // input produces a fresh stamp every tick the local
                    // copy re-decides — anchoring each one restarts the
                    // arc and the receiver never completes the jump).
                    if (body.isGrounded)
                    {
                        simState.stampActive = false;
                    }

                    if (frame.jumped && !input.IsLocalInput() &&
                        !simState.stampActive)
                    {
                        // Client-decided jump stamp (server streams only —
                        // the client's own stamp passes through locally and
                        // was already applied via jumpOrder on the tick the
                        // jump fired).  Apply the impulse anchored to the
                        // stamped lift-off position instead of re-deriving
                        // the jump from our own physics: grounded is
                        // evaluated at different ticks on each side, so
                        // re-derivation produces a different arc.
                        const float drift = math::len(body.pos - frame.jumpPos);
                        bool nearGround = body.isGrounded;
                        if (!nearGround)
                        {
                            auto groundHit =
                                terrain.CastRay(body.pos, math::vec3{0.f, -1.f, 0.f});
                            nearGround =
                                groundHit.has_value() &&
                                body.pos.y -
                                        (static_cast<float>(groundHit->hitPos.y) +
                                         1.0f) <
                                    0.6f;
                        }

                        if (drift <= kMaxJumpStampDelta && nearGround)
                        {
                            body.pos = frame.jumpPos;
                            body.velocity.y = creature.initJumpSpeed;
                            simState.stampActive = true;
                        }
                        else
                        {
                            LOG_WARN(
                                "rejected jump stamp: drift={} nearGround={} "
                                "(entity {})",
                                drift, nearGround, static_cast<uint32_t>(entity));
                        }
                        // The stamped frame also carries the held jump input
                        // — do not feed it back through the jumpOrder path.
                        creature.jumpOrder = false;
                    }
                    else
                    {
                        creature.jumpOrder = frame.jump;
                    }
                }
                else
                {
                    simState.hasFrame = false;
                }

                // Action frames — the same stamp contract and one-tick
                // delay as the movement frames (D6).
                auto& actionStream =
                    ctx.world.get<input::PlayerActionStream>(entity);
                input::PlayerActionFrame actionFrame{};
                if (input::TryReadTickFrame(actionStream, simState.actionCursor,
                                            tickCtx.currentTick,
                                            simState.lastActionTick,
                                            actionFrame))
                {
                    for (uint8_t i = 0; i < actionFrame.actionCnt; ++i)
                    {
                        ApplyRayAction(terrain, blocks, collider, body, player,
                                       actionFrame.actions[i]);
                    }
                }
            }

            // Re-apply the cached move in each sub-step (D4) — the creature
            // resets moveOrder every update.
            creature.moveOrder =
                simState.hasFrame ? simState.frame.move : math::vec3{};

            // Local streams: the creature is about to fire the impulse this
            // tick (same grounded value from the last physics tick), so
            // stamp the decision with the pre-impulse lift-off position.
            if (input.IsLocalInput() && body.isGrounded && creature.jumpOrder)
            {
                input.MarkJumpPerformed(body.pos);
            }

            player.lastActionTime -= ctx.dt;
        }
    }
}

DECL_UTYPES_IMPL(SubsystemPlayer)
}  // namespace sim
}  // namespace game
