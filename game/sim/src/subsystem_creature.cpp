#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"
#include "oge/math.hpp"

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
    auto update = [&](entt::entity e, ComponentCreature& creature,
                      ComponentPhysicBody& body)
    {
        (void)e;
        float friction =
            blocks.GetBlockFriction(blocks.GetBlockId(body.onTopOfBlkValue));

        assert(friction >= 0.0f && friction <= 1.0f);
        // assert(math::len_sq(creature.moveOrder) <= 1.0f + 1e-3f);
        if (!body.enableGravity)
        {
            body.velocity.y =
                math::lerp(body.velocity.y,
                           creature.maxSpeed * creature.moveOrder.y, friction);
        }
        body.velocity.x =
            math::lerp(body.velocity.x,
                       creature.maxSpeed * creature.moveOrder.x, friction);
        body.velocity.z =
            math::lerp(body.velocity.z,
                       creature.maxSpeed * creature.moveOrder.z, friction);

        if (body.isGrounded && creature.jumpOrder)
        {
            body.velocity.y += creature.initJumpSpeed;
        }

        creature.moveOrder = {};
        creature.jumpOrder = false;
    };

    // The realtime variant simulates only locally-predicted entities (D9):
    // remote players are never driven realtime on the client.  The fixed
    // variant keeps the tag-only filter — it simulates everyone, like the
    // server.
    if constexpr (utype == UpdateType::Realtime)
    {
        ctx.world
            .view<UpdateTag<utype>,
                  const RenderStrategyTag<RenderStrategy::LocalPrediction>,
                  ComponentCreature, ComponentPhysicBody>()
            .each(update);
    }
    else
    {
        ctx.world
            .view<UpdateTag<utype>, ComponentCreature, ComponentPhysicBody>()
            .each(update);
    }
}

DECL_UTYPES_IMPL(SubsystemCreature)
}  // namespace game::sim
