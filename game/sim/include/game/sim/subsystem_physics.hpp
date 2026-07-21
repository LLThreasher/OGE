#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/aabb_ops.hpp"

namespace game::sim
{
template<UpdateType utype>
class SubsystemPhysics : public Subsystem
{
    std::unordered_map<entt::entity, std::tuple<oge::CollisionResult2, uint32_t>> cachedCollisions;

   public:
    DECL_ID(SubsystemPhysics<utype>)
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};
}
