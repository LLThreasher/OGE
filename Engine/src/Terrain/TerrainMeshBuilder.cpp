#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Terrain/BlockManager.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::Terrain
{
	constexpr Point3 perFaceOffset[6] = {
		{0, 0, 1},
		{0, 0, -1},
		{0, 1, 0},
		{0, -1, 0},
		{1, 0, 0},
		{-1, 0, 0},
	};

	constexpr size_t oppositeFace[6] = {
		1,
		0,
		3,
		2,
		5,
		4,
	};

	constexpr std::array<uint16_t, 16> fullMask =
	{
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
		0xFFFF,
	};

	using namespace Graphics;

	TerrainMeshBuilder::TerrainMeshBuilder(TerrainData& terrain) : m_terrain(terrain), m_gpuBufferAllocator(MAX_VISIBLE_CHUNK_NUM)
	{
	}

	static size_t MaskIndex(size_t z, size_t y)
	{
		assert(0 <= y && y < 18);
		assert(0 <= z && z < 18);
		return (z * 18) + y;
	}

	void ExecuteBuildChunkMeshJob(const ChunkMeshingWorkerContext* _context, BuiltChunkMesh* context)
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
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();
					
					// -X face
					context->vertices.emplace_back(x, wy,   wz,   0, 15, 0, 0x0, 0x0);
					context->vertices.emplace_back(x, wy,   wz+1, 0, 15, 0, 0xF, 0x0);
					context->vertices.emplace_back(x, wy+1, wz+1, 0, 15, 0, 0xF, 0xF);
					context->vertices.emplace_back(x, wy+1, wz,   0, 15, 0, 0x0, 0xF);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base + 0;
					dst[1] = base + 1;
					dst[2] = base + 2;
					dst[3] = base + 2;
					dst[4] = base + 3;
					dst[5] = base + 0;
				}

				bits = negX & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// +X face
					context->vertices.emplace_back(x + 1, wy, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy, wz, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy + 1, wz, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy + 1, wz + 1, 0, 15, 0, 0, 0);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base + 0;
					dst[1] = base + 1;
					dst[2] = base + 2;
					dst[3] = base + 2;
					dst[4] = base + 3;
					dst[5] = base + 0;
				}

				bits = posY & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// +Y face
					context->vertices.emplace_back(x, wy + 1, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy + 1, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy + 1, wz, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x, wy + 1, wz, 0, 15, 0, 0, 0);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base + 0;
					dst[1] = base + 1;
					dst[2] = base + 2;
					dst[3] = base + 2;
					dst[4] = base + 3;
					dst[5] = base + 0;
				}

				bits = negY & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// -Y face
					context->vertices.emplace_back(x, wy, wz, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy, wz, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x, wy, wz + 1, 0, 15, 0, 0, 0);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base + 0;
					dst[1] = base + 1;
					dst[2] = base + 2;
					dst[3] = base + 2;
					dst[4] = base + 3;
					dst[5] = base + 0;
				}

				bits = posZ & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// +Z face
					context->vertices.emplace_back(x, wy, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x + 1, wy + 1, wz + 1, 0, 15, 0, 0, 0);
					context->vertices.emplace_back(x, wy + 1, wz + 1, 0, 15, 0, 0, 0);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base + 0;
					dst[1] = base + 1;
					dst[2] = base + 2;
					dst[3] = base + 2;
					dst[4] = base + 3;
					dst[5] = base + 0;
				}

				bits = negZ & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// -Z face
					context->vertices.emplace_back(x+1, wy,   wz, 0, 0xF, 0, 0x0, 0x0);
					context->vertices.emplace_back(x,	wy,	  wz, 0, 0xF, 0, 0xF, 0x0);
					context->vertices.emplace_back(x,	wy+1, wz, 0, 0xF, 0, 0xF, 0xF);
					context->vertices.emplace_back(x+1, wy+1, wz, 0, 0xF, 0, 0x0, 0xF);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base + 0;
					dst[1] = base + 1;
					dst[2] = base + 2;
					dst[3] = base + 2;
					dst[4] = base + 3;
					dst[5] = base + 0;
				}
			}
		}
	}

	void ExecuteBuildChunkMeshJob2(const ChunkMeshingWorkerContext* _context, BuiltChunkMesh2* context)
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
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					// -X face
					TexturedQuad quad
					{ (uint8_t)x, (uint8_t)wy, (uint8_t)wz, 0u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
					context->quads.emplace_back(quad);
				}

				bits = negX & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					// +X face
					TexturedQuad quad
					{ (uint8_t)x, (uint8_t)wy, (uint8_t)wz, 1u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
					context->quads.emplace_back(quad);
				}

				bits = posY & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					// +Y face
					TexturedQuad quad
					{ (uint8_t)x, (uint8_t)wy, (uint8_t)wz, 2u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
					context->quads.emplace_back(quad);
				}

				bits = negY & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					// -Y face
					TexturedQuad quad
					{ (uint8_t)x, (uint8_t)wy, (uint8_t)wz, 3u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
					context->quads.emplace_back(quad);
				}

				bits = posZ & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					// +Z face
					TexturedQuad quad
					{ (uint8_t)x, (uint8_t)wy, (uint8_t)wz, 4u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
					context->quads.emplace_back(quad);
				}

				bits = negZ & 0x1FFFE;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					// -Z face
					TexturedQuad quad
					{ (uint8_t)x, (uint8_t)wy, (uint8_t)wz, 5u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
					context->quads.emplace_back(quad);
				}
			}
		}
	}

	void TerrainMeshBuilder::ExecuteBuildChunkMesh(MeshingWorkerContextHandle handle)
	{
		auto _context = m_terrain.meshingWorkerContexts.Get(handle);
		auto context = m_terrain.builtChunkMeshes.Get(_context->chunkMeshHandle);
#ifdef USE_TERRAIN_MESH_V2
		ExecuteBuildChunkMeshJob2(_context, context);
#else
		ExecuteBuildChunkMeshJob(_context, context);
#endif
		m_terrain.uploadMeshQueue.push(std::make_tuple(_context->chunkHandle, _context->chunkMeshHandle));
		m_terrain.meshingWorkerContexts.Destroy(handle);
	}

	void TerrainMeshBuilder::AllocateTerrainMesh(Graphics::IGraphicsBackend& backend)
	{
#ifdef USE_TERRAIN_MESH_V2
		m_terrain.terrainMesh = backend.AllocateGPUBuffer<BufferUsage::Storage>(MAX_VISIBLE_CHUNK_NUM * CHUNK_VERTEX_BYTE_SIZE);
#else
		m_terrain.terrainMesh = {};
		m_terrain.terrainMesh.vertexBuffer = backend.AllocateGPUBuffer<BufferUsage::Vertex>(MAX_VISIBLE_CHUNK_NUM * CHUNK_VERTEX_BYTE_SIZE);
		m_terrain.terrainMesh.indexBuffer = backend.AllocateGPUBuffer<BufferUsage::Index>(MAX_VISIBLE_CHUNK_NUM * CHUNK_VERTEX_BYTE_SIZE);
#endif
	}

	void TerrainMeshBuilder::BuildChunkMeshes(int vertexBudget)
	{
		uint32_t runningVertexCount = m_terrain.meshingWorkerContexts.Size() * CHUNK_VERTEX_BYTE_SIZE;
		static_assert(std::is_default_constructible_v<ChunkMeshingWorkerContext>);
		while (runningVertexCount < vertexBudget && !m_terrain.buildMeshQueue.empty())
		{
			auto contextHandle = m_terrain.meshingWorkerContexts.Create();
			auto context = m_terrain.meshingWorkerContexts.Get(contextHandle);
			while (!m_terrain.buildMeshQueue.empty())
			{
				auto handle = std::move(m_terrain.buildMeshQueue.front());
				m_terrain.buildMeshQueue.pop();
				auto data = m_terrain.chunkData.Get(handle);
				LOG_DEBUG("building mesh at ({}, {}, {})", data->Coords.x, data->Coords.y, data->Coords.z);
				
				// skip chunks with missing neighbors
				bool incompleteNeighbors = false;
				ChunkHandle neighbors[6] = {};
				for (size_t i = 0; i < 6; ++i)
				{
					if (i == FACE_DOWN && data->Coords.y == 0)
						continue;
					auto pos = perFaceOffset[i] + data->Coords;
					auto it = m_terrain.coordToChunks.find(pos);
					if (it == m_terrain.coordToChunks.end())
					{
						incompleteNeighbors = true;
						break;
					}
					neighbors[i] = it->second;
				}

				assert(!incompleteNeighbors && "Chunk updater should prevent this");

				// build occupancy mask
				for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
				{
					for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
					{
						for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
						{
							size_t blkIdx = GetBlockIndex(x, y, z);
							assert(blkIdx < CHUNK_SIZE_TOTAL);
							uint32_t opaque = IsOpaque(data->data[blkIdx]);
							auto midx = MaskIndex(z + 1, y + 1);
							context->opaqueMasks[midx] |= opaque << (x + 1);
						}
					}
				}
				
				// handle neighbors
				// left & right
				for (size_t i = 0; i < 2; ++i)
				{
					auto neighborData = m_terrain.chunkData.Get(neighbors[i])->data;
					for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
					{
						for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
						{
							uint8_t extendedX = i == 0 ? 17 : 0;
							uint8_t neighborX = i == 0 ? 0 : 15;
							context->opaqueMasks[MaskIndex(z + 1, y + 1)] |= IsOpaque(neighborData[GetBlockIndex(neighborX, y, z)]) << extendedX;
						}
					}
				}
				
				// top & bottom
				for (size_t i = 0; i < 2; ++i)
				{
					// bottom of world
					if (i == 1 && data->Coords.y == 0)
					{
						for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
						{
							for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
							{
								uint8_t extendedY = 0;
								context->opaqueMasks[MaskIndex(z + 1, extendedY)] |= 1 << (x + 1);
							}
						}
						continue;
					}
					auto neighborData = m_terrain.chunkData.Get(neighbors[2 + i])->data;
					for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
					{
						for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
						{
							uint8_t extendedY = i == 0 ? 17 : 0;
							uint8_t neighborY = i == 0 ? 0 : 15;
							context->opaqueMasks[MaskIndex(z + 1, extendedY)] |= IsOpaque(neighborData[GetBlockIndex(x, neighborY, z)]) << (x + 1);
						}
					}
				}

				// front & back
				for (size_t i = 0; i < 2; ++i)
				{
					auto neighborData = m_terrain.chunkData.Get(neighbors[4 + i])->data;
					for (size_t y = 0; y < CHUNK_SIZE_Y; ++y)
					{
						for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
						{
							uint8_t extendedZ = i == 0 ? 17 : 0;
							uint8_t neighborZ = i == 0 ? 0 : 15;
							context->opaqueMasks[MaskIndex(extendedZ, y + 1)] |= IsOpaque(neighborData[GetBlockIndex(x, y, neighborZ)]) << (x + 1);
						}
					}
				}

				for (size_t z = 0; z < CHUNK_SIZE_Z; z++)
				{
					for (size_t y = 0; y < CHUNK_SIZE_Y; y++)
					{
						for (size_t x = 0; x < CHUNK_SIZE_X; x++)
						{
							auto index = GetBlockIndex(x, y, z);
							context->blockMetadata[index] = GetMetadata(data->data[index]);
						}
					}
				}

				context->chunkMeshHandle = m_terrain.builtChunkMeshes.Create();
				context->chunkHandle = handle;
				break;
			}
			ExecuteBuildChunkMesh(contextHandle);
		}
	}
}
