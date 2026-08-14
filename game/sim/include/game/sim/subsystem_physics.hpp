#pragma once

#include <unordered_set>

#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/aabb_ops.hpp"

namespace game::sim
{
template <UpdateType utype>
class SubsystemPhysics : public Subsystem
{
    std::unordered_map<entt::entity,
                       std::tuple<oge::CollisionResult2, uint32_t>>
        cachedCollisions;

    // D7: entities modified during the current tick's sub-steps (fixed
    // variant only).  Cleared on subStepIdx == 0, patched once on the last
    // sub-step — one replication event per entity per tick.
    std::unordered_set<entt::entity> m_modifiedTick;

   public:
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};
}  // namespace game::sim

template <game::UpdateType utype>
struct oge::runtime::TypeName<game::sim::SubsystemPhysics<utype>>
{
    static constexpr std::string Get()
    {
        return utype == UpdateType::FixedStep
                   ? "core::SubsystemPhysics<FixedStep>"
                   : "core::SubsystemPhysics<Realtime>";
    }
};
