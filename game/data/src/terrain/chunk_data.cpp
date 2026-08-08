#include <cstdint>
#include <span>
#include "game/terrain/terrain_view.hpp"
#include "oge/point3.hpp"

namespace game::terrain
{

uint32_t ChunkData::GetBlock(oge::LocalUPoint3 pt) const
{
    return GetBlock(pt.x, pt.y, pt.z);
}

uint32_t ChunkData::GetBlock(uint8_t x, uint8_t y, uint8_t z) const
{
    assert(0 <= x && x < CHUNK_SIZE_X);
    assert(0 <= y && y < CHUNK_SIZE_Y);
    assert(0 <= z && z < CHUNK_SIZE_Z);
    return data[GetBlockIndex(x, y, z)];
}

void ChunkData::SetBlock(oge::LocalUPoint3 pt, uint32_t value)
{
    SetBlock(pt.x, pt.y, pt.z, value);
}

void ChunkData::SetBlock(uint8_t x, uint8_t y, uint8_t z, uint32_t value)
{
    data[GetBlockIndex(x, y, z)] = value;
}

std::tuple<ChunkHandle, const ChunkData*> ChunkDataCollection::Get(
    Point3 coord) const
{
    auto it = coordToChunks.find(coord);
    if (it != coordToChunks.end())
    {
        return {it->second, Get(it->second)};
    }
    return {ChunkHandle{}, nullptr};
}

ChunkHandle ChunkDataCollection::GetHandle(Point3 coord) const
{
    auto it = coordToChunks.find(coord);
    if (it != coordToChunks.end())
    {
        return it->second;
    }
    return {};
}

std::tuple<ChunkHandle, ChunkData*> ChunkDataCollection::Get(Point3 coord)
{
    auto it = coordToChunks.find(coord);
    if (it != coordToChunks.end())
    {
        return {it->second, Get(it->second)};
    }
    return {ChunkHandle{}, nullptr};
}

ChunkHandle ChunkDataCollection::AllocateChunk(Point3 coord)
{
    auto it = coordToChunks.find(coord);
    if (it != coordToChunks.end())
    {
        return it->second;
    }
    auto res = chunkData.Create(coord);
    coordToChunks.emplace(coord, res);
    return res;
}

void ChunkDataCollection::FreeChunk(Point3 coord)
{
    auto it = coordToChunks.find(coord);
    if (it != coordToChunks.end())
    {
        chunkData.Destroy(it->second);
        coordToChunks.erase(it);
    }
}

void ChunkDataCollection::FreeChunk(ChunkHandle handle)
{
    auto data = chunkData.Get(handle);
    coordToChunks.erase(data->Coords);
    chunkData.Destroy(handle);
}

ChunkData* ChunkDataCollection::Poll(ChunkHandle& cursor)
{
    return chunkData.Poll(cursor);
}

const ChunkData* ChunkDataCollection::Poll(ChunkHandle& cursor) const
{
    return chunkData.Poll(cursor);
}

void PaletteCompressedChunk::FromChunkData(const ChunkData& c, PaletteCompressedChunk& result)
{
    std::unordered_map<uint32_t, uint8_t> palette_map;
    for (size_t i = 0; i < CHUNK_SIZE_TOTAL; ++i)
    {
        auto it = palette_map.find(c.data[i]);
        if (it == palette_map.end())
        {
            palette_map.emplace(c.data[i], result.palette.size());
            result.data[i] = result.palette.size();
            result.palette.push_back(c.data[i]);
        }
        else
        {
            result.data[i] = it->second;
        }
    }
    assert(result.palette.size() <= 255);
}

void PaletteCompressedChunk::ToChunkData(ChunkData& c, std::span<uint32_t> palette, std::span<uint8_t, CHUNK_SIZE_TOTAL> data)
{
    for (size_t i = 0; i < CHUNK_SIZE_TOTAL; ++i)
    {
        c.data[i] = palette[data[i]];
    }
}

void PaletteCompressedChunk::ToChunkData(ChunkData& c)
{
    ToChunkData(c, std::span<uint32_t>(palette), std::span<uint8_t, CHUNK_SIZE_TOTAL>(data));
}
}  // namespace game::terrain