#include "game/sim/subsystem_physics.hpp"

#include <unordered_map>
#include <unordered_set>

#include "game/components.hpp"
#include "game/sim/player_sim_config.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/aabb.hpp"
#include "oge/aabb_ops.hpp"

namespace game::sim
{
using oge::AABB;
using oge::CollisionResult2;

using ::game::terrain::AABBList;
using ::game::terrain::BlockRegistry;
using ::game::terrain::TerrainView;

// D9: run `fn` over every entity the variant simulates.  The realtime
// variant additionally requires the LocalPrediction tag — remote players
// are never driven realtime on the client.  The fixed variant keeps the
// tag-only filter (simulates everyone, like the server).  Two helpers
// cover the two view shapes used by the physics update.
template <UpdateType utype, typename TWorld, typename Fn>
void ForEachBody(TWorld& game, Fn&& fn)
{
    if constexpr (utype == UpdateType::Realtime)
    {
        game.template view<
                UpdateTag<utype>,
                const RenderStrategyTag<RenderStrategy::LocalPrediction>,
                ComponentPhysicBody>()
            .each(std::forward<Fn>(fn));
    }
    else
    {
        game.template view<UpdateTag<utype>, ComponentPhysicBody>().each(
            std::forward<Fn>(fn));
    }
}

template <UpdateType utype, typename TWorld, typename Fn>
void ForEachColliderBody(TWorld& game, Fn&& fn)
{
    if constexpr (utype == UpdateType::Realtime)
    {
        game.template view<
                UpdateTag<utype>,
                const RenderStrategyTag<RenderStrategy::LocalPrediction>,
                const ComponentAABBCollider, const ComponentPhysicBody>()
            .each(std::forward<Fn>(fn));
    }
    else
    {
        game.template view<UpdateTag<utype>, const ComponentAABBCollider,
                           const ComponentPhysicBody>()
            .each(std::forward<Fn>(fn));
    }
}

template <UpdateType utype>
void SubsystemPhysics<utype>::onAttach(Ctx& ctx)
{
}
template <UpdateType utype>
void SubsystemPhysics<utype>::onDetach(Ctx& ctx)
{
}

template <size_t idx>
inline bool CheckAABBAgainstTerrainSingle(float offset, const AABB& realAABB,
                                          const TerrainView& terrain,
                                          const BlockRegistry& blocks,
                                          CollisionResult2& collisionResult,
                                          uint32_t& outBlkVal)
{
    float min_z = realAABB.min.z;
    float max_z = realAABB.max.z;
    float min_y = realAABB.min.y;
    float max_y = realAABB.max.y;
    float min_x = realAABB.min.x;
    float max_x = realAABB.max.x;

    auto beginOffset = offset;
    if constexpr (idx == 0)
    {
        if (offset > 0)
        {
            min_x = max_x - 0.5f;
            max_x += offset + 0.5f;
        }
        else
        {
            max_x = min_x + 0.5f;
            min_x += offset - 0.5f;
        }
    }
    else if constexpr (idx == 1)
    {
        if (offset > 0)
        {
            min_y = max_y - 0.5f;
            max_y += offset + 0.5f;
        }
        else
        {
            max_y = min_y + 0.5f;
            min_y += offset - 0.5f;
        }
    }
    else if constexpr (idx == 2)
    {
        if (offset > 0)
        {
            min_z = max_z - 0.5f;
            max_z += offset + 0.5f;
        }
        else
        {
            max_z = min_z + 0.5f;
            min_z += offset - 0.5;
        }
    }
    else
    {
        static_assert(false);
    }

    bool hasCollision = false;
    collisionResult.effectiveOffset[idx] = offset;
    collisionResult.type = -1;
    outBlkVal = 0;

    assert(math::abs(collisionResult.effectiveOffset[idx]) <=
           math::abs(beginOffset));

    if (math::abs(collisionResult.effectiveOffset[idx]) <
        oge::COLLISION_EPSILON)
        collisionResult.effectiveOffset[idx] = 0.f;
    for (int z = math::floor(min_z); z <= math::floor(max_z); ++z)
    {
        for (int y = math::floor(min_y); y <= math::floor(max_y); ++y)
        {
            for (int x = math::floor(min_x); x <= math::floor(max_x); ++x)
            {
                uint32_t blkVal;
                AABBList blkAABBs;
                if (!terrain.TryGetBlock({x, y, z}, blkVal))
                {
                    blkAABBs = blocks.GetDefaultBlockAABBList();
                }
                else
                {
                    blkAABBs =
                        blocks.GetBlockAABBList(blocks.GetBlockId(blkVal));
                }
                for (auto blkAABB : blkAABBs)
                {
                    auto before = collisionResult.effectiveOffset[idx];
                    if (collisionResult.effectiveOffset[idx] != 0.f &&
                        CheckCollision<idx>(
                            realAABB, blkAABB + math::vec3{x, y, z},
                            collisionResult.effectiveOffset[idx],
                            collisionResult.type))
                    {
                        outBlkVal = blkVal;
                        hasCollision = true;
                    }
                    if (math::abs(collisionResult.effectiveOffset[idx]) <
                        oge::COLLISION_EPSILON)
                        collisionResult.effectiveOffset[idx] = 0.f;
                    if (before < 0.f)
                        assert(collisionResult.effectiveOffset[idx] <= 0.f);
                    else if (before > 0.f)
                        assert(collisionResult.effectiveOffset[idx] >= 0.f);
                    else
                        assert(collisionResult.effectiveOffset[idx] == 0.f);
                    assert(math::abs(collisionResult.effectiveOffset[idx]) <=
                           math::abs(before));
                }
            }
        }
    }
    // collisionResult.effectiveOffset[idx] =
    // collisionResult.effectiveOffset[idx] < COLLISION_EPSILON ? 0.f :
    // collisionResult.effectiveOffset[idx];
    assert(math::abs(collisionResult.effectiveOffset[idx]) <=
           math::abs(beginOffset));
    return hasCollision;
}

template <UpdateType utype>
void SubsystemPhysics<utype>::onUpdate(FrameCtx& ctx)
{
    // update positions with velocity
    auto& game = ctx.world;
    auto& blocks = game.ctx().get<BlockRegistry>();
    auto& terrain = game.ctx().get<TerrainView>();
    ForEachBody<utype>(game,
                       [&](entt::entity e, ComponentPhysicBody& body)
                       {
                           (void)e;
                           if (body.enableGravity)
                               body.acceleration.y -= 9.8f;
                           body.velocity += body.acceleration * ctx.dt;
                           body.acceleration = {};
                           // TEMPORARY PFX-DBG (Phase 3 impulse diagnosis —
                           // remove before commit): velocity after gravity,
                           // before collision resolution.
                           if constexpr (utype == UpdateType::Realtime)
                           {
                               if (game.ctx().contains<SimTickContext>())
                               {
                                   auto& tctx =
                                       game.ctx().get<SimTickContext>();
                                   if (tctx.currentTick >= 32 &&
                                       tctx.currentTick <= 38)
                                       fprintf(stderr,
                                               "PHYG vy=%.4f dt=%.5f\n",
                                               body.velocity.y, ctx.dt);
                               }
                           }
                       });

    // Track which entities are modified by collision resolution so we can
    // fire a single on_update / replication event per entity per frame.
    std::unordered_set<entt::entity> modified;

    cachedCollisions.clear();

    // collision between physical body and terrain Y
    ForEachColliderBody<utype>(
        game,
        [&](entt::entity e, const ComponentAABBCollider& collider,
            const ComponentPhysicBody& body)
        {
            auto& [collisions, blkVals] = cachedCollisions[e];
            auto realAABB = collider.aabb + body.pos;
            auto yOffset = body.velocity.y * ctx.dt;
            CheckAABBAgainstTerrainSingle<1>(yOffset, realAABB, terrain, blocks,
                                             collisions, blkVals);
            assert(math::abs(yOffset) >=
                   math::abs(collisions.effectiveOffset.y));
        });

    // apply collision resolution (terrain Y)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        auto& body = game.get<ComponentPhysicBody>(e);
        body.isGrounded = res.type == oge::COLLISION_TYPE_POS_Y;
        body.pos.y += res.effectiveOffset.y;

        auto& collider = game.get<ComponentAABBCollider>(e);
        auto realAABB = collider.aabb + body.pos;
        // assert(!CheckAABBAgainstTerrainOverlap(realAABB, terrain, blocks));

        if (res.type == oge::COLLISION_TYPE_POS_Y && body.velocity.y < 0.f)
            body.velocity.y = 0;
        if (res.type == oge::COLLISION_TYPE_NEG_Y && body.velocity.y > 0.f)
            body.velocity.y = 0;

        if (body.isGrounded)
            body.onTopOfBlkValue = blkVals;
        else
            body.onTopOfBlkValue = 0;

        // TEMPORARY PFX-DBG (Phase 3 impulse diagnosis — remove before
        // commit): post-Y-resolution state.
        if constexpr (utype == UpdateType::Realtime)
        {
            if (game.ctx().contains<SimTickContext>())
            {
                auto& tctx = game.ctx().get<SimTickContext>();
                if (tctx.currentTick >= 32 && tctx.currentTick <= 38)
                    fprintf(stderr, "PHYR vy=%.4f gnd=%d type=%d\n",
                            body.velocity.y, (int)body.isGrounded,
                            (int)res.type);
            }
        }

        modified.insert(e);
    }

    cachedCollisions.clear();

    // collision between physical body and terrain X
    ForEachColliderBody<utype>(
        game,
        [&](entt::entity e, const ComponentAABBCollider& collider,
            const ComponentPhysicBody& body)
        {
            auto& [collisions, blkVals] = cachedCollisions[e];
            auto realAABB = collider.aabb + body.pos;
            auto xOffset = body.velocity.x * ctx.dt;
            if (CheckAABBAgainstTerrainSingle<0>(xOffset, realAABB, terrain,
                                                 blocks, collisions, blkVals))
            {
                if (body.isGrounded && collisions.type != -1)
                {
                    uint32_t stepBlkVal;
                    CollisionResult2 stepRes;
                    CheckAABBAgainstTerrainSingle<1>(
                        -body.stepAssist,
                        realAABB + math::vec3{xOffset, body.stepAssist, 0},
                        terrain, blocks, stepRes, stepBlkVal);
                    float stepHeight =
                        body.stepAssist + stepRes.effectiveOffset.y;
                    if (stepHeight < body.stepAssist)
                    {
                        if (!CheckAABBAgainstTerrainSingle<1>(
                                stepHeight, realAABB, terrain, blocks, stepRes,
                                stepBlkVal))
                        {
                            if (!CheckAABBAgainstTerrainSingle<0>(
                                    xOffset,
                                    realAABB + math::vec3{0, stepHeight, 0},
                                    terrain, blocks, stepRes, stepBlkVal))
                            {
                                collisions.effectiveOffset = {
                                    xOffset, stepHeight, 0};
                                collisions.type = oge::COLLISION_TYPE_POS_Y;
                            }
                        }
                    }
                }
            }
        });

    // apply collision resolution (terrain X)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        auto& body = game.get<ComponentPhysicBody>(e);
        body.pos += res.effectiveOffset;

        if (res.type == oge::COLLISION_TYPE_POS_X && body.velocity.x < 0.f)
            body.velocity.x = 0;
        if (res.type == oge::COLLISION_TYPE_NEG_X && body.velocity.x > 0.f)
            body.velocity.x = 0;

        modified.insert(e);
    }

    cachedCollisions.clear();

    // collision between physical body and terrain Z
    ForEachColliderBody<utype>(
        game,
        [&](entt::entity e, const ComponentAABBCollider& collider,
            const ComponentPhysicBody& body)
        {
            auto& [collisions, blkVals] = cachedCollisions[e];
            auto realAABB = collider.aabb + body.pos;
            auto zOffset = body.velocity.z * ctx.dt;
            if (CheckAABBAgainstTerrainSingle<2>(zOffset, realAABB, terrain,
                                                 blocks, collisions, blkVals))
            {
                if (body.isGrounded && collisions.type != -1)
                {
                    uint32_t stepBlkVal;
                    CollisionResult2 stepRes;
                    CheckAABBAgainstTerrainSingle<1>(
                        -body.stepAssist,
                        realAABB + math::vec3{0, body.stepAssist, zOffset},
                        terrain, blocks, stepRes, stepBlkVal);
                    float stepHeight =
                        body.stepAssist + stepRes.effectiveOffset.y;
                    if (stepHeight < body.stepAssist)
                    {
                        if (!CheckAABBAgainstTerrainSingle<1>(
                                stepHeight, realAABB, terrain, blocks, stepRes,
                                stepBlkVal))
                        {
                            if (!CheckAABBAgainstTerrainSingle<2>(
                                    zOffset,
                                    realAABB + math::vec3{0, stepHeight, 0},
                                    terrain, blocks, stepRes, stepBlkVal))
                            {
                                collisions.effectiveOffset = {
                                    0, stepHeight, zOffset};
                                collisions.type = oge::COLLISION_TYPE_POS_Y;
                            }
                        }
                    }
                }
            }
        });

    // apply collision resolution (terrain Z)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        auto& body = game.get<ComponentPhysicBody>(e);
        body.pos += res.effectiveOffset;

        if (res.type == oge::COLLISION_TYPE_POS_Z && body.velocity.z < 0.f)
            body.velocity.z = 0;
        if (res.type == oge::COLLISION_TYPE_NEG_Z && body.velocity.z > 0.f)
            body.velocity.z = 0;

        modified.insert(e);
    }

    // --- Entity-to-Entity collision ---
    // Resolve overlapping AABBs between pairs of active physics bodies by
    // pushing apart along the axis of minimum penetration, weighted by
    // inverse mass.  No heap allocations — uses the ECS view .each()
    // directly with nested lambdas.
    {
        ForEachColliderBody<utype>(
            game,
            [&](entt::entity e1, const ComponentAABBCollider& collider1,
                const ComponentPhysicBody& body1)
            {
                auto aabb1 = collider1.aabb + body1.pos;

                ForEachColliderBody<utype>(
                    game,
                    [&](entt::entity e2, const ComponentAABBCollider& collider2,
                        const ComponentPhysicBody& body2)
                    {
                        // Only process each unordered pair once.
                        if (entt::to_integral(e2) <= entt::to_integral(e1))
                            return;

                        auto aabb2 = collider2.aabb + body2.pos;

                        if (!CheckOverlap(aabb1, aabb2)) return;

                        // Penetration depth on each axis.
                        float penX =
                            math::min(aabb1.max.x - aabb2.min.x,
                                      aabb2.max.x - aabb1.min.x);
                        float penY =
                            math::min(aabb1.max.y - aabb2.min.y,
                                      aabb2.max.y - aabb1.min.y);
                        float penZ =
                            math::min(aabb1.max.z - aabb2.min.z,
                                      aabb2.max.z - aabb1.min.z);

                        // Push apart along the axis of shallowest
                        // penetration, weighted by mass ratio.
                        float totalMass = body1.mass + body2.mass;
                        float ratio1 =
                            (totalMass > 0.f) ? body2.mass / totalMass : 0.5f;
                        float ratio2 =
                            (totalMass > 0.f) ? body1.mass / totalMass : 0.5f;

                        math::vec3 separation{};
                        int32_t collisionAxis = -1;

                        if (penX <= penY && penX <= penZ)
                        {
                            float sign = (aabb1.max.x + aabb1.min.x >
                                          aabb2.max.x + aabb2.min.x)
                                             ? 1.f
                                             : -1.f;
                            separation.x = sign * penX;
                            collisionAxis = 0;
                        }
                        else if (penY <= penZ)
                        {
                            float sign = (aabb1.max.y + aabb1.min.y >
                                          aabb2.max.y + aabb2.min.y)
                                             ? 1.f
                                             : -1.f;
                            separation.y = sign * penY;
                            collisionAxis = 1;
                        }
                        else
                        {
                            float sign = (aabb1.max.z + aabb1.min.z >
                                          aabb2.max.z + aabb2.min.z)
                                             ? 1.f
                                             : -1.f;
                            separation.z = sign * penZ;
                            collisionAxis = 2;
                        }

                        // Obtain mutable references via game.get() — the
                        // view .each() returns const refs for read-only
                        // pass, mirroring the terrain collision pattern.
                        auto& mutBody1 = game.get<ComponentPhysicBody>(e1);
                        auto& mutBody2 = game.get<ComponentPhysicBody>(e2);

                        mutBody1.pos += separation * ratio1;
                        mutBody2.pos -= separation * ratio2;

                        // Zero velocity opposing the collision normal.
                        if (collisionAxis == 0)
                        {
                            if (separation.x > 0.f &&
                                mutBody1.velocity.x < 0.f)
                                mutBody1.velocity.x = 0.f;
                            if (separation.x < 0.f &&
                                mutBody1.velocity.x > 0.f)
                                mutBody1.velocity.x = 0.f;
                            if (separation.x > 0.f &&
                                mutBody2.velocity.x > 0.f)
                                mutBody2.velocity.x = 0.f;
                            if (separation.x < 0.f &&
                                mutBody2.velocity.x < 0.f)
                                mutBody2.velocity.x = 0.f;
                        }
                        else if (collisionAxis == 1)
                        {
                            if (separation.y > 0.f &&
                                mutBody1.velocity.y < 0.f)
                                mutBody1.velocity.y = 0.f;
                            if (separation.y < 0.f &&
                                mutBody1.velocity.y > 0.f)
                                mutBody1.velocity.y = 0.f;
                            if (separation.y > 0.f &&
                                mutBody2.velocity.y > 0.f)
                                mutBody2.velocity.y = 0.f;
                            if (separation.y < 0.f &&
                                mutBody2.velocity.y < 0.f)
                                mutBody2.velocity.y = 0.f;
                        }
                        else
                        {
                            if (separation.z > 0.f &&
                                mutBody1.velocity.z < 0.f)
                                mutBody1.velocity.z = 0.f;
                            if (separation.z < 0.f &&
                                mutBody1.velocity.z > 0.f)
                                mutBody1.velocity.z = 0.f;
                            if (separation.z > 0.f &&
                                mutBody2.velocity.z > 0.f)
                                mutBody2.velocity.z = 0.f;
                            if (separation.z < 0.f &&
                                mutBody2.velocity.z < 0.f)
                                mutBody2.velocity.z = 0.f;
                        }

                        modified.insert(e1);
                        modified.insert(e2);
                    });
            });
    }

    // Fire a single on_update per modified entity so that replication hooks
    // produce at most one UpdateComponentEvent<ComponentPhysicBody> per
    // entity per update.
    //
    // D7: the fixed variant batches across the tick — one patch per entity
    // per fixed frame (kSubStepsPerTick sub-steps), so the server emits
    // exactly one replication event per entity per tick, 1:1 with the
    // client's mirror-sourced prediction.  The realtime variant keeps the
    // per-update patch (the prediction world has no replication hooks).
    // Without a SimTickContext (bare worlds, e.g. sim_physics_test) the
    // fixed variant falls back to the per-update patch.
    if constexpr (utype == UpdateType::FixedStep)
    {
        if (game.ctx().contains<SimTickContext>())
        {
            auto& tickCtx = game.ctx().get<SimTickContext>();
            if (tickCtx.subStepIdx == 0)
            {
                m_modifiedTick.clear();
            }
            m_modifiedTick.insert(modified.begin(), modified.end());
            if (tickCtx.subStepIdx == kSubStepsPerTick - 1)
            {
                for (auto e : m_modifiedTick)
                    game.patch<ComponentPhysicBody>(e);
            }
            return;
        }
    }

    for (auto e : modified)
        game.patch<ComponentPhysicBody>(e);
}

DECL_UTYPES_IMPL(SubsystemPhysics)
}  // namespace game::sim
