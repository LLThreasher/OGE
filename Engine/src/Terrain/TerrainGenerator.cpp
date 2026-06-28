#include "Engine/Terrain/Terrain.hpp"

namespace OneGame::Engine::Terrain
{
    void TerrainGenerator::GenerateTerrain(TerrainData& terrain, BlockRegistry& blocks)
    {
        int terrainGenChunkCount = 0;
        while (!terrain.generateTerrainQueue.empty() && terrainGenChunkCount < terrainGenChunkBudget)
        {
            auto handle = std::move(terrain.generateTerrainQueue.front());
            terrain.generateTerrainQueue.pop();

            // dummy generation, everything below 17 is dirt
            uint32_t dirtIdx = blocks.GetBlockId("dirt");
            auto chunk = terrain.chunks.Get(handle);
            auto chunkYBase = chunk->Coords.y << CHUNK_SHIFT_Y;
            
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                for (int y = 0; chunkYBase + y <= 17; ++y)
                {
                    for (int x = 0; x < CHUNK_SIZE_X; ++x)
                    {
                        chunk->SetBlock(x, y, z, dirtIdx);
                    }
                }
            }

            terrain.dirtyChunks.emplace(handle);
            terrainGenChunkCount += 1;
        }
    }
}  // namespace OneGame::Engine::Terrain