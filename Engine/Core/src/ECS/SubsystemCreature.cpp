#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Terrain/BlockManager.hpp"

namespace OneGame::Engine::ECS
{
    DECL_SUBSYSTEM_INIT(Creature) {}
    DECL_SUBSYSTEM_UPDATE(Creature)
    {
        auto& blocks = game.ctx().get<Terrain::BlockRegistry>();
        for (auto [e, creature, body] : game.view<ComponentCreature, ComponentPhysicBody>().each())
        {
            float friction = blocks.GetBlockFriction(blocks.GetBlockId(body.onTopOfBlkValue));
            if (math::abs(creature.moveOrder.x) > 0.01f || math::abs(creature.moveOrder.y) > 0.01f)
            {
                body.velocity = math::lerp(body.velocity, creature.maxSpeed * creature.moveOrder, friction);
            }
            else
            {
                body.velocity = math::lerp(body.velocity, math::vec3{}, friction);
            }
            if (body.isGrounded && creature.jumpOrder)
            {
                body.velocity.y += creature.initJumpSpeed;
                LOG_DEBUG("jump {}", body.velocity.y);
            }
        }
    }
}