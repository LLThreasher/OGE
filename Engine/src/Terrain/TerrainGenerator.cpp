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
            uint32_t woodIdx = blocks.GetBlockId("wood");
            uint32_t stoneIdx = blocks.GetBlockId("stone");
            auto chunk = terrain.chunks.Get(handle);
            auto chunkYBase = chunk->Coords.y << CHUNK_SHIFT_Y;

            if (chunk->Coords.x == 0 && chunk->Coords.z == 0)
            {
                for (int z = 0; z < CHUNK_SIZE_Z; ++z)
                {
                    for (int y = 0; y < CHUNK_SIZE_Y; ++y)
                    {
                        for (int x = 0; x < CHUNK_SIZE_X; ++x)
                        {
                            if ((x + y + z) % 2 == 0 && chunkYBase + y <= 17)
                                chunk->SetBlock(x, y, z, woodIdx);
                            else
                                chunk->SetBlock(x, y, z, 0);
                        }
                    }
                }
            }
            else
            {
                for (int z = 0; z < CHUNK_SIZE_Z; ++z)
                {
                    for (int y = 0; y < CHUNK_SIZE_Y; ++y)
                    {
                        for (int x = 0; x < CHUNK_SIZE_X; ++x)
                        {
                            uint32_t blkIdx;
                            if (chunkYBase + y == 17 && x == 0 && z == 0) blkIdx = stoneIdx;
                            else if (chunkYBase + y == 17 && (x == 0 || z == 0)) blkIdx = woodIdx;
                            else if (chunkYBase + y <= 17) blkIdx = dirtIdx;
                            else blkIdx = 0;

                            chunk->SetBlock(x, y, z, blkIdx);
                        }
                    }
                }
            }

            chunk->state = ChunkState::Persistent;
            terrain.dirtyChunks.emplace(handle);
            terrainGenChunkCount += 1;
        }
    }
}  // namespace OneGame::Engine::Terrain