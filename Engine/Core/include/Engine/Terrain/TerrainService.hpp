#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "BlockManager.hpp"
#include "Engine/ClassHelper.hpp"
#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainView.hpp"
#include "Engine/entt.hpp"

#define USE_TERRAIN_MESH_V2

namespace OneGame::Engine
{
class StreamingManager;
struct PresentationContext;
struct FrameOutputData;
namespace ECS
{
using TerrainContext = entt::registry;
};
}  // namespace OneGame::Engine

namespace OneGame::Engine::Terrain
{
using TerrainContext = ECS::TerrainContext;

class TerrainUpdateScheduler
{
   public:
    void InitialUpdate(TerrainData& terrain, Point3 chunkOrigin);
    void SetChunkViewDistance(int val) { m_chunkViewDistance = val; }

   private:
    int m_chunkViewDistance = 4;
};

struct TerrainDesc
{
    int chunkViewDistance = 8;
    int terrainGenChunkBudget = 8;
};

class TerrainGenerator
{
   public:
    void GenerateTerrain(TerrainData& terrain, BlockRegistry& blocks);
    void SetTerrainGenChunkBudget(int chunkBudget) { terrainGenChunkBudget = chunkBudget; }

   private:
    int terrainGenChunkBudget = 8;
};

class TerrainService : public ECS::SubsystemBase
{
   public:
    static constexpr std::string_view Name = "SubsystemTerrain";
    NO_COPY(TerrainService);
    ~TerrainService() = default;

    void Initialize(TerrainContext& ctx, AppContext actx) override;
    void Update(TerrainContext& ctx, AppContext actx, const FrameInputData& fd) override;
    // void Present(const TerrainContext& ctx, PresentationContext pctx, FrameOutputData& fd) override;

   private:
    void onPlayerCreated(entt::registry& world, entt::entity entity);
    TerrainGenerator m_terrainGenerator;
    TerrainUpdateScheduler m_terrainUpdateScheduler;
};
}  // namespace OneGame::Engine::Terrain
