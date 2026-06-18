#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Terrain/BlockManager.hpp"


namespace OneGame::Engine::Terrain
{
	static constexpr uint32_t MAX_STAGING_PER_FRAME = 6;
	static constexpr uint32_t MAX_VISIBLE_CHUNK_NUM = 160;

	static constexpr uint32_t MAXIMUM_VERTEX_BYTE_SIZE = 16 * 16 * 16 / 2 * 4 * 6 * sizeof(Vertex); // 192 kb
	static constexpr uint32_t MAXIMUM_INDEX_BYTE_SIZE = 16 * 16 * 16 / 2 * 6 * 6 * sizeof(uint16_t); // 144 kb

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

	void TerrainMeshBuilder::Initialize(AssetBundleWriter* assets)
	{
		//assets->AllocateMesh("terrainMesh", MAX_VISIBLE_CHUNK_NUM * CHUNK_VERTEX_BYTE_SIZE, MAX_VISIBLE_CHUNK_NUM * CHUNK_INDEX_BYTE_SIZE, m_terrainMesh);
	}

	static size_t MaskIndex(size_t z, size_t y)
	{
		return (z << 4) + y;
	}

	void TerrainMeshBuilder::ExecuteBuildChunkMesh(MeshingWorkerContextHandle handle)
	{
		auto _context = m_terrain.meshingWorkerContexts.Get(handle);
		auto context = m_terrain.builtChunkMeshes.Get(_context->chunkMeshHandle);
		uint32_t* masks = _context->opaqueMasks;
		for (size_t z = 1; z < CHUNK_SIZE_Z + 1; ++z)
		{
			for (size_t y = 1; y < CHUNK_SIZE_Y + 1; ++y)
			{
				uint32_t row = masks[MaskIndex(z, y)];
				uint32_t posX = row & ~(row << 1);
				uint32_t negX = row & ~(row >> 1);

				uint32_t posZ = row & ~masks[MaskIndex(z+1, y)];
				uint32_t negZ = row & ~masks[MaskIndex(z-1, y)];

				uint32_t posY = row & ~masks[MaskIndex(z, y+1)];
				uint32_t negY = row & ~masks[MaskIndex(z, y-1)];
				
				uint32_t bits;
				bits = posX;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// +X face
					context->vertices.emplace_back(x+1, wy,   wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy,   wz,    0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy+1, wz,    0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy+1, wz+1,  0,15,0,0,0);

					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base+0;
					dst[1] = base+1;
					dst[2] = base+2;
					dst[3] = base+2;
					dst[4] = base+3;
					dst[5] = base+0;
				}

				bits = negX;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// -X face
					context->vertices.emplace_back(x, wy,   wz,    0,15,0,0,0);
					context->vertices.emplace_back(x, wy,   wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x, wy+1, wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x, wy+1, wz,    0,15,0,0,0);
					
					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base+0;
					dst[1] = base+1;
					dst[2] = base+2;
					dst[3] = base+2;
					dst[4] = base+3;
					dst[5] = base+0;
				}

				bits = posY;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// +Y face
					context->vertices.emplace_back(x,   wy+1, wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy+1, wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy+1, wz,    0,15,0,0,0);
					context->vertices.emplace_back(x,   wy+1, wz,    0,15,0,0,0);
					
					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base+0;
					dst[1] = base+1;
					dst[2] = base+2;
					dst[3] = base+2;
					dst[4] = base+3;
					dst[5] = base+0;
				}
				
				bits = negY;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// -Y face
					context->vertices.emplace_back(x,   wy, wz,    0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy, wz,    0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy, wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x,   wy, wz+1,  0,15,0,0,0);
					
					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base+0;
					dst[1] = base+1;
					dst[2] = base+2;
					dst[3] = base+2;
					dst[4] = base+3;
					dst[5] = base+0;
				}
				
				bits = posZ;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// +Z face
					context->vertices.emplace_back(x,   wy,   wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy,   wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy+1, wz+1,  0,15,0,0,0);
					context->vertices.emplace_back(x,   wy+1, wz+1,  0,15,0,0,0);
					
					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base+0;
					dst[1] = base+1;
					dst[2] = base+2;
					dst[3] = base+2;
					dst[4] = base+3;
					dst[5] = base+0;
				}
				
				bits = negZ;
				while (bits)
				{
					int bx = __builtin_ctz(bits);
					bits &= bits - 1;

					int x = bx - 1;
					int wy = y - 1;
					int wz = z - 1;

					uint16_t base = (uint16_t)context->vertices.size();

					// -Z face
					context->vertices.emplace_back(x+1, wy,   wz,  0,15,0,0,0);
					context->vertices.emplace_back(x,   wy,   wz,  0,15,0,0,0);
					context->vertices.emplace_back(x,   wy+1, wz,  0,15,0,0,0);
					context->vertices.emplace_back(x+1, wy+1, wz,  0,15,0,0,0);
					
					size_t oldSize = context->indices.size();
					context->indices.resize(oldSize + 6);
					uint16_t* dst = context->indices.data() + oldSize;
					dst[0] = base+0;
					dst[1] = base+1;
					dst[2] = base+2;
					dst[3] = base+2;
					dst[4] = base+3;
					dst[5] = base+0;
				}
			}
		}
		m_terrain.uploadMeshQueue.push(std::make_tuple(_context->chunkHandle, _context->chunkMeshHandle));
		m_terrain.meshingWorkerContexts.Destroy(handle);
	}

	void TerrainMeshBuilder::BuildChunkMeshes(int vertexBudget)
	{
		uint32_t runningVertexCount = m_terrain.meshingWorkerContexts.Size() * CHUNK_VERTEX_BYTE_SIZE;
		static_assert(std::is_default_constructible_v<ChunkMeshingWorkerContext>);
		auto contextHandle = m_terrain.meshingWorkerContexts.Create();
		while (runningVertexCount < vertexBudget)
		{
			auto context = m_terrain.meshingWorkerContexts.Get(contextHandle);
			while (!m_terrain.buildMeshQueue.empty())
			{
				auto handle = m_terrain.buildMeshQueue.back();
				m_terrain.buildMeshQueue.pop();
				auto data = m_terrain.chunkData.Get(handle);
				
				// skip chunks with missing neighbors
				bool incompleteNeighbors = false;
				ChunkHandle neighbors[6] = {};
				for (size_t i = 0; i < 6; ++i)
				{
					auto it = m_terrain.coordToChunks.find(perFaceOffset[i] + data->Coords);
					if (it == m_terrain.coordToChunks.end() && !(i == FACE_DOWN && data->Coords.y == 0))
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
							context->opaqueMasks[((z + 1) << 4) + (y + 1)] |= IsOpaque(data->data[GetBlockIndex(x+1, y+1, z+1)]) << (x + 1);
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
							context->opaqueMasks[((z + 1) << 4) + (y + 1)] |= IsOpaque(neighborData[GetBlockIndex(neighborX, y, z)]) << extendedX;
						}
					}
				}
				
				// top & bottom
				for (size_t i = 0; i < 2; ++i)
				{
					// bottom of world
					if (i == 0 && data->Coords.y == 0)
					{
						for (size_t z = 0; z < CHUNK_SIZE_Z; ++z)
						{
							for (size_t x = 0; x < CHUNK_SIZE_X; ++x)
							{
								uint8_t extendedY = 0;
								context->opaqueMasks[((z + 1) << 4) + extendedY] |= 1 << (x + 1);
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
							context->opaqueMasks[((z + 1) << 4) + extendedY] |= IsOpaque(neighborData[GetBlockIndex(x, neighborY, z)]) << (x + 1);
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
							context->opaqueMasks[(extendedZ << 4) + (y + 1)] |= IsOpaque(neighborData[GetBlockIndex(x, y, neighborZ)]) << (x + 1);
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

	void TerrainMeshBuilder::UploadChunkMeshes(int byteBudget, uint32_t frameIndex, Graphics::ICommandList* cmd)
	{
		// free previous upload data and finish upload
		//for (auto& handle : m_activeStagingDataPerFrame[frameIndex])
		//{
		//	auto chunk = m_terrain.chunkData.Get(handle);
		//	auto stagingData = chunk->stagingBuffers.value();
		//	m_gpuRingStagingBuffer.Free(stagingData.vertexStagingBuffer.offset, stagingData.vertexStagingBuffer.size);
		//	m_gpuRingStagingBuffer.Free(stagingData.indexStagingBuffer.offset, stagingData.indexStagingBuffer.size);
		//	m_terrain.builtChunkMeshes.Destroy(stagingData.builtMesh);
		//	m_terrain.gpuAvailableChunkQueue.push(handle);
		//}
		//assert(byteBudget < MAX_STAGING_PER_FRAME * (CHUNK_VERTEX_BYTE_SIZE + CHUNK_INDEX_BYTE_SIZE));
		//size_t currentBytes = 0;
		//while (!m_terrain.uploadMeshQueue.empty() && currentBytes < byteBudget)
		//{
		//	auto [chunkHandle, builtMeshHandle] = m_terrain.uploadMeshQueue.back();
		//	m_terrain.uploadMeshQueue.pop();
		//	auto builtMesh = m_terrain.builtChunkMeshes.Get(builtMeshHandle);
		//	
		//	auto vertSizeInBytes = sizeof(Vertex) * builtMesh->vertices.size();
		//	auto indexSizeInBytes = sizeof(uint16_t) * builtMesh->indices.size();
		//	auto vertStaging = m_gpuRingStagingBuffer.Allocate(vertSizeInBytes);
		//	auto indexStaging = m_gpuRingStagingBuffer.Allocate(indexSizeInBytes);

		//	memcpy(vertStaging.cpuPtr, builtMesh->vertices.data(), vertSizeInBytes);
		//	memcpy(indexStaging.cpuPtr, builtMesh->indices.data(), indexSizeInBytes);

		//	int chunkOffset;
		//	if (vertSizeInBytes < CHUNK_VERTEX_BYTE_SIZE)
		//		chunkOffset = m_gpuBufferAllocator.Allocate(1);
		//	else if (vertSizeInBytes < CHUNK_VERTEX_BYTE_SIZE * 2)
		//		chunkOffset = m_gpuBufferAllocator.Allocate(2);
		//	else
		//		chunkOffset = m_gpuBufferAllocator.Allocate(4);
		//	
		//	cmd->CopyBuffer(m_gpuRingStagingBuffer.GetBuffer(), m_gpuVertexBuffer, vertSizeInBytes, vertStaging.offset, chunkOffset * CHUNK_VERTEX_BYTE_SIZE);
		//	cmd->CopyBuffer(m_gpuRingStagingBuffer.GetBuffer(), m_gpuIndexBuffer, indexSizeInBytes, indexStaging.offset, chunkOffset * CHUNK_INDEX_BYTE_SIZE);

		//	auto chunk = m_terrain.chunkData.Get(chunkHandle);
		//	chunk->stagingBuffers = {
		//		builtMeshHandle,
		//		vertStaging,
		//		indexStaging,
		//	};
		//	m_activeStagingDataPerFrame[frameIndex].push_back(chunkHandle);
		//	currentBytes += vertSizeInBytes + indexSizeInBytes;
		//}
	}
}
