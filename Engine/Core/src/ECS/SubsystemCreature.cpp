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

            if (!body.enableGravity)
            {
                body.velocity.y = math::lerp(body.velocity.y, creature.maxSpeed * creature.moveOrder.y, friction);
            }
            body.velocity.x = math::lerp(body.velocity.x, creature.maxSpeed * creature.moveOrder.x, friction);
            body.velocity.z = math::lerp(body.velocity.z, creature.maxSpeed * creature.moveOrder.z, friction);
            
            if (body.isGrounded && creature.jumpOrder)
            {
                body.velocity.y += creature.initJumpSpeed;
            }
        }
    }
}