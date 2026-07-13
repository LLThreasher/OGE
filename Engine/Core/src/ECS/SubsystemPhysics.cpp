#include <unordered_map>

#include "Engine/AABBOps.hpp"
#include "Engine/ECS/Components.hpp"
#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Formaters.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainView.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemPhysics::Initialize(GameWorldContext& game, AppContext ctx) {}

template <size_t idx>
inline bool CheckAABBAgainstTerrainSingle(float offset, const AABB& realAABB, const Terrain::TerrainView& terrain,
                                          const Terrain::BlockRegistry& blocks, CollisionResult2& collisionResult,
                                          uint32_t& blkVal)
{
    float min_z = realAABB.min.z;
    float max_z = realAABB.max.z;
    float min_y = realAABB.min.y;
    float max_y = realAABB.max.y;
    float min_x = realAABB.min.x;
    float max_x = realAABB.max.x;

    // if (offset > 0)
    // {
    //     offset = math::max(offset, COLLISION_EPSILON);
    // }
    // else if (offset < 0)
    // {
    //     offset = math::min(offset, -COLLISION_EPSILON);
    // }

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
    blkVal = 0;
    
    assert(math::abs(collisionResult.effectiveOffset[idx]) <= math::abs(beginOffset));
    
    if (math::abs(collisionResult.effectiveOffset[idx]) < COLLISION_EPSILON)
        collisionResult.effectiveOffset[idx] = 0.f;
    for (int z = math::floor(min_z); z <= math::floor(max_z); ++z)
    {
        for (int y = math::floor(min_y); y <= math::floor(max_y); ++y)
        {
            for (int x = math::floor(min_x); x <= math::floor(max_x); ++x)
            {
                uint32_t blkVal;
                Terrain::AABBList blkAABBs;
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
                    if (collisionResult.effectiveOffset[idx] != 0.f && CheckCollision<idx>(realAABB, blkAABB + math::vec3{x, y, z},
                                            collisionResult.effectiveOffset[idx], collisionResult.type))
                    {
                        blkVal = blkVal;
                        hasCollision = true;
                    }
                    if (math::abs(collisionResult.effectiveOffset[idx]) < COLLISION_EPSILON)
                        collisionResult.effectiveOffset[idx] = 0.f;
                    if (before < 0.f) assert(collisionResult.effectiveOffset[idx] <= 0.f);
                    else if (before > 0.f) assert(collisionResult.effectiveOffset[idx] >= 0.f);
                    else assert(collisionResult.effectiveOffset[idx] == 0.f);
                    assert(math::abs(collisionResult.effectiveOffset[idx]) <= math::abs(before));
                }
            }
        }
    }
    // collisionResult.effectiveOffset[idx] = collisionResult.effectiveOffset[idx] < COLLISION_EPSILON ? 0.f : collisionResult.effectiveOffset[idx];
    assert(math::abs(collisionResult.effectiveOffset[idx]) <= math::abs(beginOffset));
    return hasCollision;
}

inline bool CheckAABBAgainstTerrainStepAssist(int32_t collisionType, const AABB& realAABB, const Terrain::TerrainView& terrain,
                                              const Terrain::BlockRegistry& blocks, float& penetration,
                                              uint32_t& blkVal)
{
    float min_z = realAABB.min.z - COLLISION_EPSILON;
    float max_z = realAABB.max.z + COLLISION_EPSILON;
    float min_y = realAABB.min.y - COLLISION_EPSILON;
    float max_y = realAABB.max.y + COLLISION_EPSILON;
    float min_x = realAABB.min.x - COLLISION_EPSILON;
    float max_x = realAABB.max.x + COLLISION_EPSILON;

    if (collisionType == 0)
    {
        max_x += 0.5f;
        min_x -= 0.5f;
    }
    else if (collisionType == 1)
    {
        max_x += 0.5f;
        min_x -= 0.5f;
    }
    max_x += 0.5f;
    min_x -= 0.5f;

    bool hasCollision = false;
    penetration = 0.f;
    blkVal = 0;
    for (int z = math::floor(min_z); z <= math::floor(max_z); ++z)
    {
        for (int y = math::floor(min_y); y <= math::floor(max_y); ++y)
        {
            for (int x = math::floor(min_x); x <= math::floor(max_x); ++x)
            {
                uint32_t blkVal;
                Terrain::AABBList blkAABBs;
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
                    if (CheckCollisionRejection<2>(realAABB, blkAABB + math::vec3{x, y, z}, penetration))
                    {
                        blkVal = blkVal;
                        hasCollision = true;
                    }
                }
            }
        }
    }
    return hasCollision;
}

inline bool CheckAABBAgainstTerrainOverlap(const AABB& realAABB, const Terrain::TerrainView& terrain,
                                           const Terrain::BlockRegistry& blocks)
{
    float min_z = realAABB.min.z - COLLISION_EPSILON;
    float max_z = realAABB.max.z + COLLISION_EPSILON;
    float min_y = realAABB.min.y - COLLISION_EPSILON;
    float max_y = realAABB.max.y + COLLISION_EPSILON;
    float min_x = realAABB.min.x - COLLISION_EPSILON;
    float max_x = realAABB.max.x + COLLISION_EPSILON;

    for (int z = math::floor(min_z); z <= math::floor(max_z); ++z)
    {
        for (int y = math::floor(min_y); y <= math::floor(max_y); ++y)
        {
            for (int x = math::floor(min_x); x <= math::floor(max_x); ++x)
            {
                uint32_t blkVal;
                Terrain::AABBList blkAABBs;
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
                    if (CheckOverlap(realAABB, blkAABB + math::vec3{x, y, z}))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void SubsystemPhysics::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    // update positions with velocity
    auto& blocks = game.ctx().get<Terrain::BlockRegistry>();
    auto& terrain = game.ctx().get<Terrain::TerrainView>();
    for (auto [e, body] : game.view<ComponentPhysicBody>().each())
    {
        body.acceleration.y -= 9.8f;

        body.velocity += body.acceleration * fd.dt;

        body.acceleration = {};
    }

    std::unordered_map<entt::entity, std::tuple<CollisionResult2, uint32_t>> cachedCollisions;

    // collision between physical body and terrain Y
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        auto yOffset = body.velocity.y * fd.dt;
        CheckAABBAgainstTerrainSingle<1>(yOffset, realAABB, terrain, blocks, collisions, blkVals);
        assert(math::abs(yOffset) >= math::abs(collisions.effectiveOffset.y));
    }

    // apply collision resolution (terrain Y)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        auto& body = game.get<ComponentPhysicBody>(e);
        body.isGrounded = res.type == COLLISION_TYPE_POS_Y;
        body.pos.y += res.effectiveOffset.y;

        auto& collider = game.get<ComponentAABBCollider>(e);
        auto realAABB = collider.aabb + body.pos;
        // assert(!CheckAABBAgainstTerrainOverlap(realAABB, terrain, blocks));

        if (res.type == COLLISION_TYPE_POS_Y && body.velocity.y < 0.f) body.velocity.y = 0;
        if (res.type == COLLISION_TYPE_NEG_Y && body.velocity.y > 0.f) body.velocity.y = 0;

        if (body.isGrounded)
            body.onTopOfBlkValue = blkVals;
        else
            body.onTopOfBlkValue = 0;
    }

    cachedCollisions.clear();

    // collision between physical body and terrain X
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        auto xOffset = body.velocity.x * fd.dt;
        if (CheckAABBAgainstTerrainSingle<0>(xOffset, realAABB, terrain, blocks, collisions, blkVals))
        {
            if (body.isGrounded && collisions.type != -1)
            {
                uint32_t stepBlkVal;
                CollisionResult2 stepRes;
                CheckAABBAgainstTerrainSingle<1>(-body.stepAssist, realAABB + math::vec3{xOffset, body.stepAssist, 0}, terrain, blocks, stepRes, stepBlkVal);
                float stepHeight = body.stepAssist + stepRes.effectiveOffset.y;
                if (stepHeight < body.stepAssist)
                {
                    if (!CheckAABBAgainstTerrainSingle<1>(stepHeight, realAABB, terrain, blocks, stepRes,
                    stepBlkVal))
                    {
                        if (!CheckAABBAgainstTerrainSingle<0>(xOffset, realAABB + math::vec3{0, stepHeight, 0},
                        terrain, blocks, stepRes, stepBlkVal))
                        {
                            collisions.effectiveOffset = {xOffset, stepHeight, 0};
                            collisions.type = COLLISION_TYPE_POS_Y;
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

        if (res.type == COLLISION_TYPE_POS_X && body.velocity.x < 0.f) body.velocity.x = 0;
        if (res.type == COLLISION_TYPE_NEG_X && body.velocity.x > 0.f) body.velocity.x = 0;
    }

    cachedCollisions.clear();

    // collision between physical body and terrain X
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        auto zOffset = body.velocity.z * fd.dt;
        if (CheckAABBAgainstTerrainSingle<2>(zOffset, realAABB, terrain, blocks, collisions, blkVals))
        {
            if (body.isGrounded && collisions.type != -1)
            {
                uint32_t stepBlkVal;
                CollisionResult2 stepRes;
                CheckAABBAgainstTerrainSingle<1>(-body.stepAssist, realAABB + math::vec3{0, body.stepAssist, zOffset}, terrain, blocks, stepRes, stepBlkVal);
                float stepHeight = body.stepAssist + stepRes.effectiveOffset.y;
                if (stepHeight < body.stepAssist)
                {
                    if (!CheckAABBAgainstTerrainSingle<1>(stepHeight, realAABB, terrain, blocks, stepRes,
                    stepBlkVal))
                    {
                        if (!CheckAABBAgainstTerrainSingle<2>(zOffset, realAABB + math::vec3{0, stepHeight, 0},
                        terrain, blocks, stepRes, stepBlkVal))
                        {
                            collisions.effectiveOffset = {0, stepHeight, zOffset};
                            collisions.type = COLLISION_TYPE_POS_Y;
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

        if (res.type == COLLISION_TYPE_POS_Z && body.velocity.z < 0.f) body.velocity.z = 0;
        if (res.type == COLLISION_TYPE_NEG_Z && body.velocity.z > 0.f) body.velocity.z = 0;
    }
}
}  // namespace OneGame::Engine::ECS
