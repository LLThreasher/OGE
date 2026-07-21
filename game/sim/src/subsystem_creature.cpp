#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"

namespace game::sim
{
using game::sim::SubsystemCreature;

void SubsystemCreature::onAttach(Ctx& ctx) {}
void SubsystemCreature::onDetach(Ctx& ctx) {}

void SubsystemCreature::onUpdate(FrameCtx& ctx)
{
    auto& blocks = ctx.world.ctx().get<terrain::BlockRegistry>();
    for (auto [e, creature, body] : ctx.world.view<ComponentCreature, ComponentPhysicBody>().each())
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

        creature.moveOrder = {};
        creature.jumpOrder = false;
    }
}
}  // namespace game::sim