#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"

namespace game::sim
{
using game::sim::SubsystemCreature;

template <UpdateType utype>
void SubsystemCreature<utype>::onAttach(Ctx& ctx)
{
}
template <UpdateType utype>
void SubsystemCreature<utype>::onDetach(Ctx& ctx)
{
}

template <UpdateType utype>
void SubsystemCreature<utype>::onUpdate(FrameCtx& ctx)
{
    auto& blocks = ctx.world.ctx().get<terrain::BlockRegistry>();
    for (auto [e, creature, body] : ctx.world.view<UpdateTag<utype>, ComponentCreature, ComponentPhysicBody>().each())
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

DECL_UTYPES_IMPL(SubsystemCreature)
}  // namespace game::sim
