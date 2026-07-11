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

bool CheckAABBAgainstTerrain(float stepAssist, const AABB& realAABB, const Terrain::TerrainView& terrain,
                             const Terrain::BlockRegistry& blocks, std::vector<CollisionResult>& collisions, std::vector<uint32_t>& blkVals)
{
    for (int z = math::floor(realAABB.min.z); z <= math::floor(realAABB.max.z); ++z)
    {
        for (int y = math::floor(realAABB.min.y); y <= math::floor(realAABB.max.y); ++y)
        {
            for (int x = math::floor(realAABB.min.x); x <= math::floor(realAABB.max.x); ++x)
            {
                uint32_t blkVal;
                if (!terrain.TryGetBlock({x, y, z}, blkVal)) return false;
                for (auto blkAABB : blocks.GetBlockAABBList(blocks.GetBlockId(blkVal)))
                {
                    CollisionResult res{};
                    if (CheckCollision(realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                    if (res.type != COLLISION_TYPE_POS_Y && CheckStepAssistCollision(stepAssist, realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                }
            }
        }
    }
    return true;
}

bool CheckAABBAgainstTerrainY(float stepAssist, const AABB& realAABB, const Terrain::TerrainView& terrain,
                             const Terrain::BlockRegistry& blocks, std::vector<CollisionResult>& collisions, std::vector<uint32_t>& blkVals)
{
    for (int z = math::floor(realAABB.min.z); z <= math::floor(realAABB.max.z); ++z)
    {
        for (int y = math::floor(realAABB.min.y); y <= math::floor(realAABB.max.y); ++y)
        {
            for (int x = math::floor(realAABB.min.x); x <= math::floor(realAABB.max.x); ++x)
            {
                uint32_t blkVal;
                if (!terrain.TryGetBlock({x, y, z}, blkVal)) return false;
                for (auto blkAABB : blocks.GetBlockAABBList(blocks.GetBlockId(blkVal)))
                {
                    CollisionResult res{};
                    if (CheckCollision(realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                }
            }
        }
    }
    return true;
}

bool CheckAABBAgainstTerrainX(float stepAssist, const AABB& realAABB, const Terrain::TerrainView& terrain,
                             const Terrain::BlockRegistry& blocks, std::vector<CollisionResult>& collisions, std::vector<uint32_t>& blkVals)
{
    for (int y = math::floor(realAABB.min.y); y <= math::floor(realAABB.max.y); ++y)
    {
        for (int z = math::floor(realAABB.min.z); z <= math::floor(realAABB.max.z); ++z)
        {
            for (int x = math::floor(realAABB.min.x); x <= math::floor(realAABB.max.x); ++x)
            {
                uint32_t blkVal;
                if (!terrain.TryGetBlock({x, y, z}, blkVal)) return false;
                for (auto blkAABB : blocks.GetBlockAABBList(blocks.GetBlockId(blkVal)))
                {
                    CollisionResult res{};
                    if (CheckCollisionXZ(realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                    if (stepAssist > 0.f && CheckStepAssistCollision(stepAssist, realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                }
            }
        }
    }
    return true;
}

bool CheckAABBAgainstTerrainZ(float stepAssist, const AABB& realAABB, const Terrain::TerrainView& terrain,
                             const Terrain::BlockRegistry& blocks, std::vector<CollisionResult>& collisions, std::vector<uint32_t>& blkVals)
{
    for (int y = math::floor(realAABB.min.y); y <= math::floor(realAABB.max.y); ++y)
    {
        for (int z = math::floor(realAABB.min.z); z <= math::floor(realAABB.max.z); ++z)
        {
            for (int x = math::floor(realAABB.min.x); x <= math::floor(realAABB.max.x); ++x)
            {
                uint32_t blkVal;
                if (!terrain.TryGetBlock({x, y, z}, blkVal)) return false;
                for (auto blkAABB : blocks.GetBlockAABBList(blocks.GetBlockId(blkVal)))
                {
                    CollisionResult res{};
                    if (CheckCollisionZ(realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                    if (CheckStepAssistCollision(stepAssist, realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                        blkVals.push_back(blkVal);
                    }
                }
            }
        }
    }
    return true;
}

bool CheckAABBAgainstTerrainRevStep(const AABB& realAABB, const Terrain::TerrainView& terrain,
                                    const Terrain::BlockRegistry& blocks)
{
    for (int z = math::floor(realAABB.min.z); z <= math::floor(realAABB.max.z); ++z)
    {
        for (int y = math::floor(realAABB.min.y); y <= math::floor(realAABB.max.y); ++y)
        {
            for (int x = math::floor(realAABB.min.x); x <= math::floor(realAABB.max.x); ++x)
            {
                uint32_t blkVal;
                if (!terrain.TryGetBlock({x, y, z}, blkVal)) return true;
                for (auto blkAABB : blocks.GetBlockAABBList(blocks.GetBlockId(blkVal)))
                {
                    CollisionResult res{};
                    if (CheckCollision(realAABB, blkAABB + math::vec3{x, y, z}, res))
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

        // LOG_DEBUG("on top of {}", body.onTopOfBlkValue);
        // auto drag = blocks.GetBlockDrag(blocks.GetBlockId(body.onTopOfBlkValue));
        float drag = 5.0f;
        body.velocity.x *= (1.0f / (1.0f + drag * fd.dt));
        body.velocity.z *= (1.0f / (1.0f + drag * fd.dt));

        float dy = body.velocity.y * fd.dt;
        body.pos.y += math::abs(dy) < COLLISION_EPSILON ? 0.f : dy;
    }

    std::unordered_map<entt::entity, std::tuple<std::vector<CollisionResult>, std::vector<uint32_t>>> cachedCollisions;
    
    // collision between physical body and terrain Y
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        CollisionResult res{};
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        if (!CheckAABBAgainstTerrainY(body.stepAssist, realAABB, terrain, blocks, collisions, blkVals))
        {
            cachedCollisions.erase(e);
        }
    }

    // apply collision resolution (terrain Y)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        ResolveCollisionResult result{};
        auto& body = game.get<ComponentPhysicBody>(e);
        auto& aabb = game.get<ComponentAABBCollider>(e).aabb;
        ResolveCollisionsY(res, result);
        body.isGrounded = (result.mask & RCR_HIT_POS_Y) != 0 && result.offset.y < COLLISION_EPSILON * 3.f;
        
        body.pos += result.offset;
        if ((result.mask & RCR_HIT_POS_Y) != 0 && body.velocity.y < 0.f) body.velocity.y = 0;
        if ((result.mask & RCR_HIT_NEG_Y) != 0 && body.velocity.y > 0.f) body.velocity.y = 0;

        body.acceleration = {0.f, 0.f, 0.f};
        if (body.isGrounded && result.resolvedIndices[1] != -1)
            body.onTopOfBlkValue = blkVals[result.resolvedIndices[1]];
        else
            body.onTopOfBlkValue = 0;
    }

    for (auto [e, body] : game.view<ComponentPhysicBody>().each())
    {
        float dx = body.velocity.x * fd.dt;
        body.pos.x += math::abs(dx) < COLLISION_EPSILON ? 0.f : dx;
    }

    cachedCollisions.clear();

    // collision between physical body and terrain X
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        CollisionResult res{};
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        if (!CheckAABBAgainstTerrainX(body.isGrounded ? body.stepAssist : 0.f, realAABB, terrain, blocks, collisions, blkVals))
        {
            cachedCollisions.erase(e);
        }
    }

    // apply collision resolution (terrain XZ)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        ResolveCollisionResult result{};
        auto& body = game.get<ComponentPhysicBody>(e);
        auto& aabb = game.get<ComponentAABBCollider>(e).aabb;
        ResolveCollisionsX(res, result);

        auto aabbAfterStep =
            aabb + body.pos + COLLISION_NORMALS[COLLISION_TYPE_STEP_Y] * result.stepOffset;
        aabbAfterStep.min.y = math::max(aabbAfterStep.min.y, aabb.max.y + body.pos.y);
        if ((result.mask & RCR_HIT_STEP_Y) != 0 &&
            (result.stepOffset < math::ceil(aabb.max.y) - aabb.max.y || !CheckAABBAgainstTerrainRevStep(aabbAfterStep, terrain, blocks)))
        {
            // body.pos += math::vec3{result.offset.x, result.stepOffset, result.offset.z};
            // body.pos += math::vec3{-result.offset.x, result.stepOffset, -result.offset.z};
            // LOG_INFO("step up {} {} {} {} {} {}", result.stepOffset, result.mask, result.offset, body.velocity, body.acceleration, fd.dt);
            // auto forward = math::normalize(math::vec2{body.velocity.x, body.velocity.z}) * COLLISION_EPSILON * 100.f;
            body.pos += math::vec3{result.offset.x, result.stepOffset, result.offset.z};
            body.velocity.y = 0;
            // get block the body is on top of
        }
        else
        {
            body.pos.x += result.offset.x;
            if ((result.mask & RCR_HIT_POS_X) != 0 && body.velocity.x < 0.f) body.velocity.x = 0;
            if ((result.mask & RCR_HIT_NEG_X) != 0 && body.velocity.x > 0.f) body.velocity.x = 0;
        }
    }

    for (auto [e, body] : game.view<ComponentPhysicBody>().each())
    {
        float dz = body.velocity.z * fd.dt;
        body.pos.z += math::abs(dz) < COLLISION_EPSILON ? 0.f : dz;
    }

    cachedCollisions.clear();

    // collision between physical body and terrain Z
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        CollisionResult res{};
        auto& [collisions, blkVals] = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        if (!CheckAABBAgainstTerrainZ(body.isGrounded ? body.stepAssist : 0.f, realAABB, terrain, blocks, collisions, blkVals))
        {
            cachedCollisions.erase(e);
        }
    }

    // apply collision resolution (terrain Z)
    for (auto [e, tup] : cachedCollisions)
    {
        auto& [res, blkVals] = tup;
        ResolveCollisionResult result{};
        auto& body = game.get<ComponentPhysicBody>(e);
        auto& aabb = game.get<ComponentAABBCollider>(e).aabb;
        ResolveCollisionsZ(res, result);

        auto aabbAfterStep =
            aabb + body.pos + COLLISION_NORMALS[COLLISION_TYPE_STEP_Y] * result.stepOffset;
        aabbAfterStep.min.y = math::max(aabbAfterStep.min.y, aabb.max.y + body.pos.y);
        if ((result.mask & RCR_HIT_STEP_Y) != 0 &&
            (result.stepOffset < math::ceil(aabb.max.y) - aabb.max.y || !CheckAABBAgainstTerrainRevStep(aabbAfterStep, terrain, blocks)))
        {
            // body.pos += math::vec3{result.offset.x, result.stepOffset, result.offset.z};
            // body.pos += math::vec3{-result.offset.x, result.stepOffset, -result.offset.z};
            // LOG_INFO("Z step up {} {} {} {} {} {}", result.stepOffset, math::ceil(aabb.max.y) - aabb.max.y, result.offset, body.velocity, body.acceleration, fd.dt);
            // auto forward = math::normalize(math::vec2{body.velocity.x, body.velocity.z}) * COLLISION_EPSILON * 100.f;
            body.pos += math::vec3{result.offset.x, result.stepOffset, result.offset.z};
            body.velocity.y = 0;
            // get block the body is on top of
        }
        else
        {
            body.pos += result.offset;
            if ((result.mask & RCR_HIT_POS_Z) != 0 && body.velocity.z < 0.f) body.velocity.z = 0;
            if ((result.mask & RCR_HIT_NEG_Z) != 0 && body.velocity.z > 0.f) body.velocity.z = 0;
        }
    }
}
}  // namespace OneGame::Engine::ECS
