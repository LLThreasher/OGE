#include "game/sim/terrain/subsystem_terrain.hpp"

#include <limits>

#include "game/components.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/terrain_view.hpp"

namespace game::sim::terrain
{
void SubsystemTerrain::onAttach(GameState& ctx)
{
    if (!ctx.world.ctx().contains<TerrainDesc>()) ctx.world.ctx().emplace<TerrainDesc>();
    auto desc = ctx.world.ctx().get<TerrainDesc>();
    m_terrainGenerator.SetTerrainGenChunkBudget(desc.terrainGenChunkBudget);
    m_terrainUpdateScheduler.SetChunkViewDistance(desc.chunkViewDistance);
    m_resolveDirtyChunkConnection =
        ctx.events.sink<ResolveDirtyChunkEvent>().connect<&TerrainView::HandleResolveDirtyChunk>(ctx.world);
    m_createPlayerConnection =
        ctx.world.on_construct<ComponentPlayer>().connect<&SubsystemTerrain::onPlayerCreated>(this);
}

void SubsystemTerrain::onDetach(GameState& ctx)
{
    m_resolveDirtyChunkConnection.release();
    m_createPlayerConnection.release();
}

void SubsystemTerrain::onPlayerCreated(entt::registry& world, entt::entity entity)
{
    auto pos = world.get<ComponentCamera>(entity).position;
    Point3 ipos = {math::floor(pos.x) / CHUNK_SIZE_X, math::floor(pos.y) / CHUNK_SIZE_Y,
                   math::floor(pos.z) / CHUNK_SIZE_Z};
    m_terrainUpdateScheduler.InitialUpdate(world.ctx().get<TerrainView>().m_terrainData, ipos);
}

void SubsystemTerrain::onUpdate(FGameState& ctx)
{
    m_terrainGenerator.GenerateTerrain(ctx.world.ctx().get<TerrainView>().m_terrainData,
                                       ctx.world.ctx().get<BlockRegistry>());
}

void TerrainUpdateScheduler::InitialUpdate(TerrainData& terrain, Point3 chunkOrigin)
{
    int radius = m_chunkViewDistance + 1;

    int x = 0;
    int z = 0;

    int dx = 0;
    int dz = -1;

    int maxSide = radius * 2 + 1;
    int maxSteps = maxSide * maxSide;

    for (int i = 0; i < maxSteps; ++i)
    {
        if (std::abs(x) <= radius && std::abs(z) <= radius)
        {
            for (int y = 0; y <= 4; ++y)
            {
                Point3 coord = {chunkOrigin.x + x, y, chunkOrigin.z + z};

                auto handle = terrain.chunks.AllocateChunk(coord);
                terrain.generateTerrainQueue.push(handle);
            }
        }

        // Spiral turn logic
        if (x == z || (x < 0 && x == -z) || (x > 0 && x == 1 - z))
        {
            int temp = dx;
            dx = -dz;
            dz = temp;
        }

        x += dx;
        z += dz;
    }
}

}  // namespace game::sim::terrain
