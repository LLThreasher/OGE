#pragma once

namespace OneGame::Engine::Terrain
{

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 16;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_SIZE_TOTAL = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
constexpr int CHUNK_SHIFT_X = 0;
constexpr int CHUNK_SHIFT_Y = 4;
constexpr int CHUNK_SHIFT_Z = 8;

}  // namespace OneGame::Engine::Terrain
