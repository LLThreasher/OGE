#include <unordered_map>

#include "Engine/ECS/Components.hpp"
#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Formaters.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainView.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemPhysics::Initialize(GameWorldContext& game, AppContext ctx) {}

bool CheckAABBAgainstTerrain(float stepAssist, const AABB& realAABB, const Terrain::TerrainView& terrain,
                             const Terrain::BlockRegistry& blocks, std::vector<CollisionResult>& collisions)
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
                    CollisionResult res;
                    if (CheckCollision(realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
                    }
                    if (CheckStepAssistCollision(stepAssist, realAABB, blkAABB + math::vec3{x, y, z}, res))
                    {
                        collisions.push_back(res);
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
                    CollisionResult res;
                    if (CheckInverseStepAssistCollision(realAABB, blkAABB + math::vec3{x, y, z}))
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
    for (auto [e, collider, body] : game.view<ComponentAABBCollider, ComponentPhysicBody>().each())
    {
        body.acceleration.y -= 9.8f;

        body.velocity += body.acceleration * fd.dt;

        body.velocity.x *= (1.0f / (1.0f + body.drag * fd.dt));
        body.velocity.z *= (1.0f / (1.0f + body.drag * fd.dt));

        auto delta = body.velocity * fd.dt;
        delta.x = math::abs(delta.x) < COLLISION_EPSILON ? 0.f : delta.x;
        delta.y = math::abs(delta.y) < COLLISION_EPSILON ? 0.f : delta.y;
        delta.z = math::abs(delta.z) < COLLISION_EPSILON ? 0.f : delta.z;
        body.pos += delta;
        body.acceleration = {0.f, 0.f, 0.f};
    }

    auto& terrain = game.ctx().get<Terrain::TerrainView>();
    auto& blocks = game.ctx().get<Terrain::BlockRegistry>();
    std::unordered_map<entt::entity, std::vector<CollisionResult>> cachedCollisions;
    // collision between physical body and terrain
    for (auto [e, collider, body] : game.view<const ComponentAABBCollider, const ComponentPhysicBody>().each())
    {
        CollisionResult res;
        auto& collisions = cachedCollisions[e];
        auto realAABB = collider.aabb + body.pos;
        if (!CheckAABBAgainstTerrain(body.stepAssist, realAABB, terrain, blocks, collisions))
        {
            cachedCollisions.erase(e);
        }
    }

    // apply collision resolution
    for (auto [e, res] : cachedCollisions)
    {
        ResolveCollisionResult result;
        auto& body = game.get<ComponentPhysicBody>(e);
        auto& aabb = game.get<ComponentAABBCollider>(e).aabb;
        ResolveCollisionsSinglePass(res, result);
        auto aabbAfterStep =
            aabb + body.pos + COLLISION_NORMALS[COLLISION_TYPE_STEP_Y] * (result.stepOffset + COLLISION_EPSILON);
        if ((result.mask & RCR_HIT_POS_Y) != 0) body.isGrounded = true;
        if ((result.mask & RCR_HIT_STEP_Y) != 0 && !CheckAABBAgainstTerrainRevStep(aabbAfterStep, terrain, blocks))
        {
            body.pos += math::vec3{result.offset.x, result.stepOffset, result.offset.z};
            body.velocity.y = 0;
        }
        else
        {
            body.pos += result.offset;
            if ((result.mask & RCR_HIT_POS_Y) != 0 && body.velocity.y < 0.f) body.velocity.y = 0;
            if ((result.mask & RCR_HIT_NEG_Y) != 0 && body.velocity.y > 0.f) body.velocity.y = 0;

            if ((result.mask & RCR_HIT_POS_X) != 0 && body.velocity.x < 0.f) body.velocity.x = 0;
            if ((result.mask & RCR_HIT_POS_Z) != 0 && body.velocity.z < 0.f) body.velocity.z = 0;

            if ((result.mask & RCR_HIT_NEG_X) != 0 && body.velocity.x > 0.f) body.velocity.x = 0;
            if ((result.mask & RCR_HIT_NEG_Z) != 0 && body.velocity.z > 0.f) body.velocity.z = 0;
        }
    }
}
}  // namespace OneGame::Engine::ECS
