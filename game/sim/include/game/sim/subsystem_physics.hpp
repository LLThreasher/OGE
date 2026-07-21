#include "game/sim/subsystem.hpp"
#include "oge/aabb_ops.hpp"

namespace game::sim
{
class SubsystemPhysics : public RealtimeSystem
{
    std::unordered_map<entt::entity, std::tuple<oge::CollisionResult2, uint32_t>> cachedCollisions;
    SubsystemPhysicsData m_data;

   public:
    DECL_ID(SubsystemPhysics)
    SubsystemPhysics() : m_data({}), RealtimeSystem(Id) {}
    SubsystemPhysics(SubsystemPhysicsData data) : m_data(data), RealtimeSystem(Id) {}
    static std::unique_ptr<RealtimeSystem> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<SubsystemPhysics>(def.subsystemPhysicsData);
    }
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};
}
