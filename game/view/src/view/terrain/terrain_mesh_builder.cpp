#include <bit>

#include "game/terrain/block_registry.hpp"
#include "game/view/terrain/terrain_renderer.hpp"
#include "oge/log.hpp"

namespace game::view::terrain
{

constexpr size_t oppositeFace[6] = {
    1, 0, 3, 2, 5, 4,
};

constexpr std::array<uint16_t, 16> fullMask = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
};

using namespace oge::graphics;

static size_t MaskIndex(size_t z, size_t y)
{
    assert(0 <= y && y < 18);
    assert(0 <= z && z < 18);
    return (z * 18) + y;
}

inline int32_t MaskHasBlock(const uint32_t* masks, int32_t x, int32_t y, int32_t z)
{
    return (masks[MaskIndex(z + 1, y + 1)] & (1 << (x + 1))) != 0 ? 1 : 0;
}

inline uint8_t VertexAO(uint8_t corner, uint8_t s1, uint8_t s2) { return (s1 & s2) ? 3 : (s1 + s2 + corner); }

void ExecuteBuildChunkMeshJob2(const ChunkMeshingWorkerContext* _context, BuiltChunkMesh2* context,
                               const BlockRegistry& blocks)
{
    const uint32_t* masks = _context->opaqueMasks;
    for (size_t z = 1; z < CHUNK_SIZE_Z + 1; ++z)
    {
        for (size_t y = 1; y < CHUNK_SIZE_Y + 1; ++y)
        {
            uint32_t row = masks[MaskIndex(z, y)];
            uint32_t posX = row & ~(row << 1);
            uint32_t negX = row & ~(row >> 1);

            uint32_t posZ = row & ~masks[MaskIndex(z + 1, y)];
            uint32_t negZ = row & ~masks[MaskIndex(z - 1, y)];

            uint32_t posY = row & ~masks[MaskIndex(z, y + 1)];
            uint32_t negY = row & ~masks[MaskIndex(z, y - 1)];

            uint32_t bits;
            bits = posX & 0x1FFFE;
            while (bits)
            {
                int bx = std::countr_zero(bits);
                bits &= bits - 1;

                int x = bx - 1;
                int wy = y - 1;
                int wz = z - 1;

                uint8_t texSlot = blocks.GetTextureSlot(blocks.GetBlockId(_context->compressedChunk.Get(x, wy, wz)))[0];

                // -X face
                TexturedQuad quad{
                    (uint8_t)x,
                    (uint8_t)wy,
                    (uint8_t)wz,
                    0u,
                    texSlot,
                    COLOR_WHITE,
                    {0xF, 0xF, 0xF, 0xF},
                    {
                        VertexAO(MaskHasBlock(masks, x - 1, wy - 1, wz - 1), MaskHasBlock(masks, x - 1, wy - 1, wz),
                                 MaskHasBlock(masks, x - 1, wy, wz - 1)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy - 1, wz + 1), MaskHasBlock(masks, x - 1, wy - 1, wz),
                                 MaskHasBlock(masks, x - 1, wy, wz + 1)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy + 1, wz + 1), MaskHasBlock(masks, x - 1, wy + 1, wz),
                                 MaskHasBlock(masks, x - 1, wy, wz + 1)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy + 1, wz - 1), MaskHasBlock(masks, x - 1, wy + 1, wz),
                                 MaskHasBlock(masks, x - 1, wy, wz - 1)),
                    }};
                context->quads.emplace_back(quad);
            }

            bits = negX & 0x1FFFE;
            while (bits)
            {
                int bx = std::countr_zero(bits);
                bits &= bits - 1;

                int x = bx - 1;
                int wy = y - 1;
                int wz = z - 1;

                uint8_t texSlot = blocks.GetTextureSlot(blocks.GetBlockId(_context->compressedChunk.Get(x, wy, wz)))[1];

                // +X face
                TexturedQuad quad{
                    (uint8_t)x,
                    (uint8_t)wy,
                    (uint8_t)wz,
                    1u,
                    texSlot,
                    COLOR_WHITE,
                    {0xF, 0xF, 0xF, 0xF},
                    {
                        VertexAO(MaskHasBlock(masks, x + 1, wy - 1, wz + 1), MaskHasBlock(masks, x + 1, wy - 1, wz),
                                 MaskHasBlock(masks, x + 1, wy, wz + 1)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy - 1, wz - 1), MaskHasBlock(masks, x + 1, wy - 1, wz),
                                 MaskHasBlock(masks, x + 1, wy, wz - 1)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy + 1, wz - 1), MaskHasBlock(masks, x + 1, wy + 1, wz),
                                 MaskHasBlock(masks, x + 1, wy, wz - 1)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy + 1, wz + 1), MaskHasBlock(masks, x + 1, wy + 1, wz),
                                 MaskHasBlock(masks, x + 1, wy, wz + 1)),
                    }};
                context->quads.emplace_back(quad);
            }

            bits = posY & 0x1FFFE;
            while (bits)
            {
                int bx = std::countr_zero(bits);
                bits &= bits - 1;

                int x = bx - 1;
                int wy = y - 1;
                int wz = z - 1;

                uint8_t texSlot = blocks.GetTextureSlot(blocks.GetBlockId(_context->compressedChunk.Get(x, wy, wz)))[2];

                // +Y face
                TexturedQuad quad{
                    (uint8_t)x,
                    (uint8_t)wy,
                    (uint8_t)wz,
                    2u,
                    texSlot,
                    COLOR_WHITE,
                    {0xF, 0xF, 0xF, 0xF},
                    {
                        VertexAO(MaskHasBlock(masks, x - 1, wy + 1, wz + 1), MaskHasBlock(masks, x, wy + 1, wz + 1),
                                 MaskHasBlock(masks, x - 1, wy + 1, wz)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy + 1, wz + 1), MaskHasBlock(masks, x, wy + 1, wz + 1),
                                 MaskHasBlock(masks, x + 1, wy + 1, wz)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy + 1, wz - 1), MaskHasBlock(masks, x, wy + 1, wz - 1),
                                 MaskHasBlock(masks, x + 1, wy + 1, wz)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy + 1, wz - 1), MaskHasBlock(masks, x, wy + 1, wz - 1),
                                 MaskHasBlock(masks, x - 1, wy + 1, wz)),
                    }};
                context->quads.emplace_back(quad);
            }

            bits = negY & 0x1FFFE;
            while (bits)
            {
                int bx = std::countr_zero(bits);
                bits &= bits - 1;

                int x = bx - 1;
                int wy = y - 1;
                int wz = z - 1;

                uint8_t texSlot = blocks.GetTextureSlot(blocks.GetBlockId(_context->compressedChunk.Get(x, wy, wz)))[3];

                // -Y face
                TexturedQuad quad{
                    (uint8_t)x,
                    (uint8_t)wy,
                    (uint8_t)wz,
                    3u,
                    texSlot,
                    COLOR_WHITE,
                    {0xF, 0xF, 0xF, 0xF},
                    {
                        VertexAO(MaskHasBlock(masks, x - 1, wy - 1, wz - 1), MaskHasBlock(masks, x, wy - 1, wz - 1),
                                 MaskHasBlock(masks, x - 1, wy - 1, wz)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy - 1, wz + 1), MaskHasBlock(masks, x, wy - 1, wz + 1),
                                 MaskHasBlock(masks, x + 1, wy - 1, wz)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy - 1, wz - 1), MaskHasBlock(masks, x, wy - 1, wz - 1),
                                 MaskHasBlock(masks, x + 1, wy - 1, wz)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy - 1, wz + 1), MaskHasBlock(masks, x, wy - 1, wz + 1),
                                 MaskHasBlock(masks, x - 1, wy - 1, wz)),
                    }};
                context->quads.emplace_back(quad);
            }

            bits = posZ & 0x1FFFE;
            while (bits)
            {
                int bx = std::countr_zero(bits);
                bits &= bits - 1;

                int x = bx - 1;
                int wy = y - 1;
                int wz = z - 1;

                uint8_t texSlot = blocks.GetTextureSlot(blocks.GetBlockId(_context->compressedChunk.Get(x, wy, wz)))[4];

                // +Z face
                TexturedQuad quad{
                    (uint8_t)x,
                    (uint8_t)wy,
                    (uint8_t)wz,
                    4u,
                    texSlot,
                    COLOR_WHITE,
                    {0xF, 0xF, 0xF, 0xF},
                    {
                        VertexAO(MaskHasBlock(masks, x - 1, wy - 1, wz + 1), MaskHasBlock(masks, x, wy - 1, wz + 1),
                                 MaskHasBlock(masks, x - 1, wy, wz + 1)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy - 1, wz + 1), MaskHasBlock(masks, x, wy - 1, wz + 1),
                                 MaskHasBlock(masks, x + 1, wy, wz + 1)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy + 1, wz + 1), MaskHasBlock(masks, x, wy + 1, wz + 1),
                                 MaskHasBlock(masks, x + 1, wy, wz + 1)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy + 1, wz + 1), MaskHasBlock(masks, x, wy + 1, wz + 1),
                                 MaskHasBlock(masks, x - 1, wy, wz + 1)),
                    }};
                context->quads.emplace_back(quad);
            }

            bits = negZ & 0x1FFFE;
            while (bits)
            {
                int bx = std::countr_zero(bits);
                bits &= bits - 1;

                int x = bx - 1;
                int wy = y - 1;
                int wz = z - 1;

                uint8_t texSlot = blocks.GetTextureSlot(blocks.GetBlockId(_context->compressedChunk.Get(x, wy, wz)))[5];

                // -Z face
                TexturedQuad quad{
                    (uint8_t)x,
                    (uint8_t)wy,
                    (uint8_t)wz,
                    5u,
                    texSlot,
                    COLOR_WHITE,
                    {0xF, 0xF, 0xF, 0xF},
                    {
                        VertexAO(MaskHasBlock(masks, x + 1, wy - 1, wz - 1), MaskHasBlock(masks, x, wy - 1, wz - 1),
                                 MaskHasBlock(masks, x + 1, wy, wz - 1)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy - 1, wz - 1), MaskHasBlock(masks, x, wy - 1, wz - 1),
                                 MaskHasBlock(masks, x - 1, wy, wz - 1)),
                        VertexAO(MaskHasBlock(masks, x - 1, wy + 1, wz - 1), MaskHasBlock(masks, x, wy + 1, wz - 1),
                                 MaskHasBlock(masks, x - 1, wy, wz - 1)),
                        VertexAO(MaskHasBlock(masks, x + 1, wy + 1, wz - 1), MaskHasBlock(masks, x, wy + 1, wz - 1),
                                 MaskHasBlock(masks, x + 1, wy, wz - 1)),
                    }};
                context->quads.emplace_back(quad);
            }
        }
    }
}

void TerrainMeshBuilder::ExecuteBuildChunkMesh(TerrainPresentationData& terrain, MeshingWorkerContextHandle handle,
                                               const BlockRegistry& blocks)
{
    m_runningVertexCount += CHUNK_VERTEX_BYTE_SIZE;
    auto _context = terrain.meshingWorkerContexts.Get(handle);
    auto context = terrain.builtChunkMeshes.Get(_context->chunkMeshHandle);
    ExecuteBuildChunkMeshJob2(_context, context, blocks);
    if (!terrain.builtChunkMeshes.Get(_context->chunkMeshHandle)->quads.empty())
        terrain.uploadMeshQueue.push(std::make_tuple(_context->chunkHandle, _context->chunkMeshHandle));
    else
        terrain.builtChunkMeshes.Destroy(_context->chunkMeshHandle);
    terrain.meshingWorkerContexts.Destroy(handle);
    m_runningVertexCount -= CHUNK_VERTEX_BYTE_SIZE;
}

void TerrainMeshBuilder::BuildChunkMeshes(const TerrainData& terrain, const BlockRegistry& blocks,
                                          TerrainPresentationData& pData)
{
    while (!pData.buildMeshQueue.empty())
    {
        if (m_runningVertexCount > m_vertexBudget) break;
        auto contextHandle = pData.meshingWorkerContexts.Create();
        auto context = pData.meshingWorkerContexts.Get(contextHandle);

        auto handle = std::move(pData.buildMeshQueue.front());
        pData.buildMeshQueue.pop();
        auto data = terrain.chunks.Get(handle);
        // LOG_DEBUG("building mesh at ({}, {}, {})", data->Coords.x, data->Coords.y, data->Coords.z);

        // skip chunks with missing neighbors
        const ChunkData* neighbors[6] = {};
        for (size_t i = 0; i < 6; ++i)
        {
            if (i == FACE_DOWN && data->Coords.y == 0) continue;
            auto pos = oge::perFaceOffset[i] + data->Coords;
            auto [_, chunk] = terrain.chunks.Get(pos);
            neighbors[i] = chunk;
        }

        // assert(!incompleteNeighbors && "Chunk updater should prevent this");

        for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
        {
            for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
            {
                auto midx = MaskIndex(z + 1, y + 1);
                context->opaqueMasks[midx] = 0;
                for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
                {
                    size_t blkIdx = GetBlockIndex(x, y, z);
                    assert(blkIdx < CHUNK_SIZE_TOTAL);
                    uint32_t opaque = blocks.IsOpaque(blocks.GetBlockId(data->data[blkIdx]));
                    context->opaqueMasks[midx] |= opaque << (x + 1);
                }
            }
        }

        // handle neighbors
        // left & right
        for (size_t i = 0; i < 2; ++i)
        {
            uint8_t extendedX = i == 0 ? 17 : 0;
            if (!neighbors[i])
            {
                for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
                {
                    for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
                    {
                        context->opaqueMasks[MaskIndex(z + 1, y + 1)] |= 1 << extendedX;
                    }
                }
            }
            else
            {
                auto neighborData = neighbors[i]->data;
                for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
                {
                    for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
                    {
                        uint8_t neighborX = i == 0 ? 0 : 15;
                        auto midx = MaskIndex(z + 1, y + 1);
                        context->opaqueMasks[midx] |=
                            blocks.IsOpaque(blocks.GetBlockId(neighborData[GetBlockIndex(neighborX, y, z)]))
                            << extendedX;
                    }
                }
            }
        }

        // top & bottom
        for (size_t i = 0; i < 2; ++i)
        {
            uint8_t extendedY = i == 0 ? 17 : 0;
            // bottom of world
            if (!neighbors[2 + i] || i == 1 && data->Coords.y == 0)
            {
                for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
                {
                    auto midx = MaskIndex(z + 1, extendedY);
                    context->opaqueMasks[midx] = 0;
                    for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
                    {
                        context->opaqueMasks[midx] |= 1 << (x + 1);
                    }
                }
            }
            else
            {
                auto neighborData = neighbors[2 + i]->data;
                for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
                {
                    auto midx = MaskIndex(z + 1, extendedY);
                    context->opaqueMasks[midx] = 0;
                    for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
                    {
                        uint8_t neighborY = i == 0 ? 0 : 15;
                        context->opaqueMasks[midx] |=
                            blocks.IsOpaque(blocks.GetBlockId(neighborData[GetBlockIndex(x, neighborY, z)])) << (x + 1);
                    }
                }
            }
        }

        // front & back
        for (size_t i = 0; i < 2; ++i)
        {
            uint8_t extendedZ = i == 0 ? 17 : 0;
            if (!neighbors[4 + i])
            {
                for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
                {
                    auto midx = MaskIndex(extendedZ, y + 1);
                    context->opaqueMasks[midx] = 0;
                    for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
                    {
                        uint8_t neighborZ = i == 0 ? 0 : 15;
                        context->opaqueMasks[midx] |= 1 << (x + 1);
                    }
                }
            }
            else
            {
                auto neighborData = neighbors[4 + i]->data;
                for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
                {
                    auto midx = MaskIndex(extendedZ, y + 1);
                    context->opaqueMasks[midx] = 0;
                    for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
                    {
                        uint8_t neighborZ = i == 0 ? 0 : 15;
                        context->opaqueMasks[midx] |= blocks.IsOpaque(neighborData[GetBlockIndex(x, y, neighborZ)])
                                                      << (x + 1);
                    }
                }
            }
        }

        // build occupancy mask
        uint32_t allAndMask = 0x1FFFE;
        uint32_t allOrMask = 0;
        for (size_t z = 0; z < CHUNK_SIZE_Z + 2; ++z)
        {
            for (size_t y = 0; y < CHUNK_SIZE_Y + 2; ++y)
            {
                auto midx = MaskIndex(z, y);
                allAndMask &= context->opaqueMasks[midx];
                allOrMask |= context->opaqueMasks[midx];
            }
        }

        if (allAndMask == 0x1FFFE)
        {
            // LOG_DEBUG("skip chunk meshing for ({}, {}, {}), because it is full soild", data->Coords.x,
            //           data->Coords.y, data->Coords.z);
            pData.meshingWorkerContexts.Destroy(contextHandle);
            continue;
        }
        else if (allOrMask == 0x0)
        {
            // LOG_DEBUG("skip chunk meshing for ({}, {}, {}), because it is full air", data->Coords.x,
            // data->Coords.y,
            //           data->Coords.z);
            pData.meshingWorkerContexts.Destroy(contextHandle);
            continue;
        }

        context->compressedChunk = PaletteCompressedChunk::FromChunkData(*data);

        context->chunkMeshHandle = pData.builtChunkMeshes.Create();
        context->chunkHandle = handle;
        ExecuteBuildChunkMesh(pData, contextHandle, blocks);
    }
}
}  // namespace game::view::terrain
