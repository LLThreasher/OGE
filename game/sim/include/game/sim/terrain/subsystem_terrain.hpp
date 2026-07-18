#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "game/sim/subsystem.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/macros.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/entt.hpp"

namespace game::sim
{

namespace terrain
{
using namespace oge::runtime;
using namespace game::terrain;
class TerrainUpdateScheduler
{
   public:
    void InitialUpdate(TerrainData& terrain, Point3 chunkOrigin);
    void SetChunkViewDistance(int val) { m_chunkViewDistance = val; }

   private:
    int m_chunkViewDistance = 4;
};

class TerrainGenerator
{
   public:
    void GenerateTerrain(TerrainData& terrain, BlockRegistry& blocks);
    void SetTerrainGenChunkBudget(int chunkBudget) { terrainGenChunkBudget = chunkBudget; }

   private:
    int terrainGenChunkBudget = 8;
};

class SubsystemTerrain : Subsystem
{
   public:
    static constexpr oge_id_type Id = entt::hashed_string("SubsystemTerrain").value();
    SubsystemTerrain() : Subsystem(Id) {}
    NO_COPY(SubsystemTerrain);
    ~SubsystemTerrain() = default;

    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
    // void Present(const TerrainContext& ctx, PresentationContext pctx, FrameOutputData& fd) override;

   private:
    void onPlayerCreated(entt::registry& world, entt::entity entity);
    TerrainGenerator m_terrainGenerator;
    TerrainUpdateScheduler m_terrainUpdateScheduler;
    entt::connection m_resolveDirtyChunkConnection;
    entt::connection m_createPlayerConnection;
};
}  // namespace terrain
using SubsystemTerrain = terrain::SubsystemTerrain;
}  // namespace game::sim
