#include "game/sim/subsystem_physics.hpp"

#include <unordered_map>

#include "game/components.hpp"
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

template <UpdateType utype>
void SubsystemPhysics<utype>::onAttach(Ctx& ctx)
{
}
template <UpdateType utype>
void SubsystemPhysics<utype>::onDetach(Ctx& ctx)
{
}

template <size_t idx>
inline bool CheckAABBAgainstTerrainSingle(float offset, const AABB& realAABB, const TerrainView& terrain,
                                          const BlockRegistry& blocks, CollisionResult2& collisionResult,
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

    assert(math::abs(collisionResult.effectiveOffset[idx]) <= math::abs(beginOffset));

    if (math::abs(collisionResult.effectiveOffset[idx]) < oge::COLLISION_EPSILON)
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
                    blkAABBs = blocks.GetBlockAABBList(blocks.GetBlockId(blkVal));
                }
                for (auto blkAABB : blkAABBs)
                {
                    auto before = collisionResult.effectiveOffset[idx];
                    if (collisionResult.effectiveOffset[idx] != 0.f &&
                        CheckCollision<idx>(realAABB, blkAABB + math::vec3{x, y, z},
                                            collisionResult.effectiveOffset[idx], collisionResult.type))
                    {
                        outBlkVal = blkVal;
                        hasCollision = true;
                    }
                    if (math::abs(collisionResult.effectiveOffset[idx]) < oge::COLLISION_EPSILON)
                        collisionResult.effectiveOffset[idx] = 0.f;
                    if (before < 0.f)
                        assert(collisionResult.effectiveOffset[idx] <= 0.f);
                    else if (before > 0.f)
                        assert(collisionResult.effectiveOffset[idx] >= 0.f);
                    else
                        assert(collisionResult.effectiveOffset[idx] == 0.f);
                    assert(math::abs(collisionResult.effectiveOffset[idx]) <= math::abs(before));
                }
            }
        }
    }
    // collisionResult.effectiveOffset[idx] = collisionResult.effectiveOffset[idx] < COLLISION_EPSILON ? 0.f :
    // collisionResult.effectiveOffset[idx];
    assert(math::abs(collisionResult.effectiveOffset[idx]) <= math::abs(beginOffset));
    return hasCollision;
}

template <UpdateType utype>
void SubsystemPhysics<utype>::onUpdate(FrameCtx& ctx)
{
    // update positions with velocity
    auto& game = ctx.world;
    auto& blocks = game.ctx().get<BlockRegistry>();
    auto& terrain = game.ctx().get<TerrainView>();
    for (auto [e, body] : game.view<UpdateTag<utype>, ComponentPhysicBody>().each())
    {
        if (body.enableGravity) body.acceleration.y -= 9.8f;
        body.velocity += body.acceleration * ctx.dt;
        body.acceleration = {};
    }

    // collision between physical body and terrain Y
    for (auto [e, collider, body] :
         game.view<UpdateTag<utype>, const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        auto yOffset = body.velocity.y * ctx.dt;
        CheckAABBAgainstTerrainSingle<1>(yOffset, realAABB, terrain, blocks, collisions, blkVals);
        assert(math::abs(yOffset) >= math::abs(collisions.effectiveOffset.y));
    }

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

        if (res.type == oge::COLLISION_TYPE_POS_Y && body.velocity.y < 0.f) body.velocity.y = 0;
        if (res.type == oge::COLLISION_TYPE_NEG_Y && body.velocity.y > 0.f) body.velocity.y = 0;

        if (body.isGrounded)
            body.onTopOfBlkValue = blkVals;
        else
            body.onTopOfBlkValue = 0;
    }

    cachedCollisions.clear();

    // collision between physical body and terrain X
    for (auto [e, collider, body] :
         game.view<UpdateTag<utype>, const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        auto xOffset = body.velocity.x * ctx.dt;
        if (CheckAABBAgainstTerrainSingle<0>(xOffset, realAABB, terrain, blocks, collisions, blkVals))
        {
            if (body.isGrounded && collisions.type != -1)
            {
                uint32_t stepBlkVal;
                CollisionResult2 stepRes;
                CheckAABBAgainstTerrainSingle<1>(-body.stepAssist, realAABB + math::vec3{xOffset, body.stepAssist, 0},
                                                 terrain, blocks, stepRes, stepBlkVal);
                float stepHeight = body.stepAssist + stepRes.effectiveOffset.y;
                if (stepHeight < body.stepAssist)
                {
                    if (!CheckAABBAgainstTerrainSingle<1>(stepHeight, realAABB, terrain, blocks, stepRes, stepBlkVal))
                    {
                        if (!CheckAABBAgainstTerrainSingle<0>(xOffset, realAABB + math::vec3{0, stepHeight, 0}, terrain,
                                                              blocks, stepRes, stepBlkVal))
                        {
                            collisions.effectiveOffset = {xOffset, stepHeight, 0};
                            collisions.type = oge::COLLISION_TYPE_POS_Y;
                        }
                    }
                }
            }
        }
    }

    // apply collision resolution (terrain X)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        auto& body = game.get<ComponentPhysicBody>(e);
        body.pos += res.effectiveOffset;

        if (res.type == oge::COLLISION_TYPE_POS_X && body.velocity.x < 0.f) body.velocity.x = 0;
        if (res.type == oge::COLLISION_TYPE_NEG_X && body.velocity.x > 0.f) body.velocity.x = 0;
    }

    cachedCollisions.clear();

    // collision between physical body and terrain Z
    for (auto [e, collider, body] :
         game.view<UpdateTag<utype>, const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        auto zOffset = body.velocity.z * ctx.dt;
        if (CheckAABBAgainstTerrainSingle<2>(zOffset, realAABB, terrain, blocks, collisions, blkVals))
        {
            if (body.isGrounded && collisions.type != -1)
            {
                uint32_t stepBlkVal;
                CollisionResult2 stepRes;
                CheckAABBAgainstTerrainSingle<1>(-body.stepAssist, realAABB + math::vec3{0, body.stepAssist, zOffset},
                                                 terrain, blocks, stepRes, stepBlkVal);
                float stepHeight = body.stepAssist + stepRes.effectiveOffset.y;
                if (stepHeight < body.stepAssist)
                {
                    if (!CheckAABBAgainstTerrainSingle<1>(stepHeight, realAABB, terrain, blocks, stepRes, stepBlkVal))
                    {
                        if (!CheckAABBAgainstTerrainSingle<2>(zOffset, realAABB + math::vec3{0, stepHeight, 0}, terrain,
                                                              blocks, stepRes, stepBlkVal))
                        {
                            collisions.effectiveOffset = {0, stepHeight, zOffset};
                            collisions.type = oge::COLLISION_TYPE_POS_Y;
                        }
                    }
                }
            }
        }
    }

    // apply collision resolution (terrain Z)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        auto& body = game.get<ComponentPhysicBody>(e);
        body.pos += res.effectiveOffset;

        if (res.type == oge::COLLISION_TYPE_POS_Z && body.velocity.z < 0.f) body.velocity.z = 0;
        if (res.type == oge::COLLISION_TYPE_NEG_Z && body.velocity.z > 0.f) body.velocity.z = 0;
    }
}

DECL_UTYPES_IMPL(SubsystemPhysics)
}  // namespace game::sim
