#pragma once

#include <cassert>
#include <array>
#include <unordered_set>
#include <queue>
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Graphics/ChunkAllocator.hpp"
#include "BlockManager.hpp"

namespace OneGame::Engine::Terrain
{
	constexpr int CHUNK_SIZE_X = 16;
	constexpr int CHUNK_SIZE_Y = 16;
	constexpr int CHUNK_SIZE_Z = 16;
	constexpr int CHUNK_SIZE_TOTAL = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
	constexpr int CHUNK_SHIFT_X = 0;
	constexpr int CHUNK_SHIFT_Y = 4;
	constexpr int CHUNK_SHIFT_Z = 8;

	enum class TerrainObject
	{
		Chunk,
		BuiltChunkMesh,
		MeshingWorkerContext,
	};

	using ChunkHandle = ResourceHandle<TerrainObject::Chunk>;
	using BuiltMeshHandle = ResourceHandle<TerrainObject::BuiltChunkMesh>;
	using MeshingWorkerContextHandle = ResourceHandle<TerrainObject::MeshingWorkerContext>;

	struct LocalPoint3
	{
		int8_t x, y, z;
	};

	struct Point3
	{
		int32_t x, y, z;

		bool operator==(const Point3& other) const noexcept
		{
			return x == other.x &&
				y == other.y &&
				z == other.z;
		}

		Point3 operator+(const Point3& other) const noexcept
		{
			return {
				x + other.x,
				y + other.y,
				z + other.z
			};
		}

		Point3 operator-(const Point3& other) const noexcept
		{
			return {
				x - other.x,
				y - other.y,
				z - other.z
			};
		}
	};

	struct Point3Hash
	{
		size_t operator()(const Point3& p) const noexcept
		{
			size_t hx = std::hash<int32_t>{}(p.x);
			size_t hy = std::hash<int32_t>{}(p.y);
			size_t hz = std::hash<int32_t>{}(p.z);

			// Mix the hashes
			size_t seed = hx;
			seed ^= hy + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= hz + 0x9e3779b9 + (seed << 6) + (seed >> 2);

			return seed;
		}
	};

	enum class ChunkState
	{
		GeneratingTerrain,
		GeneratingMesh,
		PendingUpload,
		GpuAvailable,
		PendingDestroy,
	};

	size_t GetBlockIndex(uint8_t x, uint8_t y, uint8_t z)
	{
		return (x << CHUNK_SHIFT_X) + (y << CHUNK_SHIFT_Y) + (z << CHUNK_SHIFT_Z);
	}

	struct BuiltChunkMesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
	};

	struct StagingData
	{
		BuiltMeshHandle builtMesh;
		Graphics::StagingAllocation vertexStagingBuffer;
		Graphics::StagingAllocation indexStagingBuffer;
	};

	struct AllocatedChunkSlot
	{
		uint32_t index;
		uint32_t size; // always 1, 2, 4
	};

	struct ChunkData
	{
		uint32_t data[CHUNK_SIZE_TOTAL] = {};
		Point3 Coords = {};
		std::optional<StagingData> stagingBuffers;
		std::optional<AllocatedChunkSlot> chunkMeshSlot;

	public:
		uint32_t GetBlock(uint8_t x, uint8_t y, uint8_t z)
		{
			assert(0 <= x && x < CHUNK_SIZE_X);
			assert(0 <= y && y < CHUNK_SIZE_Y);
			assert(0 <= z && z < CHUNK_SIZE_Z);
			return data[GetBlockIndex(x, y, z)];
		}

		void SetBlock(uint8_t x, uint8_t y, uint8_t z, uint32_t value)
		{
			data[GetBlockIndex(x, y, z)] = value;
		}
	};

	struct LocalUpdateBlockCmd
	{
		uint32_t value;
		LocalPoint3 coord;
		bool touchesBorder;
	};

	struct BuiltMesh
	{
		ChunkHandle handle;
		uint32_t vertexOffset;
		uint32_t vertexSize;
		uint32_t indexOffset;
		uint32_t indexSize;
	};

	struct ChunkMeshingWorkerContext
	{
		// 1296 bytes
		uint32_t opaqueMasks[(CHUNK_SIZE_Y + 2) * (CHUNK_SHIFT_Z + 2)];
		// 8192 bytes
		BlockMetadata blockMetadata[CHUNK_SIZE_TOTAL];
		ChunkHandle chunkHandle;
		BuiltMeshHandle chunkMeshHandle;
	};

	struct TerrainData
	{
		std::unordered_map<Point3, ChunkHandle, Point3Hash> coordToChunks;
		std::queue<Point3> destroyChunkQueue;
		std::queue<ChunkHandle> generateTerrainQueue;
		std::queue<ChunkHandle> buildMeshQueue;
		std::queue<std::tuple<ChunkHandle, BuiltMeshHandle>> uploadMeshQueue;
		std::queue<ChunkHandle> gpuAvailableChunkQueue;
		std::unordered_map<Point3, std::vector<LocalUpdateBlockCmd>, Point3Hash> blockModificationQueue;
		ResourcePool<TerrainObject::Chunk, ChunkData> chunkData;
		ResourcePool<TerrainObject::BuiltChunkMesh, BuiltChunkMesh> builtChunkMeshes;
		ResourcePool<TerrainObject::MeshingWorkerContext, ChunkMeshingWorkerContext> meshingWorkerContexts;
	};

	class TerrainMeshBuilder
	{
	public:
		void Initialize(Graphics::IGraphicsBackend*, TerrainData& terrain);
		void Shutdown(Graphics::IGraphicsBackend*);
		void BuildChunkMeshes(int vertexBudget);
		void UploadChunkMeshes(int byteBudget, uint32_t frameIndex, Graphics::ICommandList* cmd);

		std::tuple<GPUBufferHandle, GPUBufferHandle> GetGpuBuffers()
		{
			return {m_gpuVertexBuffer, m_gpuIndexBuffer};
		}
	private:
		void ExecuteBuildChunkMesh(MeshingWorkerContextHandle);

		TerrainData& m_terrain;
		Graphics::ChunkAllocator m_gpuBufferAllocator;
		GPUBufferHandle m_gpuVertexBuffer;
		GPUBufferHandle m_gpuIndexBuffer;
		Graphics::RingStagingBuffer m_gpuRingStagingBuffer;
		std::vector<std::vector<ChunkHandle>> m_activeStagingDataPerFrame;
	};

	class TerrainUpdateScheduler
	{
	public:
		void UpdateAndCullChunks(float dt, Point3 chunkOrigin);
		std::unordered_set<AllocatedChunkSlot>& GetCurrentVisibleChunks();
	private:
		std::unordered_set<AllocatedChunkSlot> currentVisibleChunks;
	};

	struct TerrainGenerationDesc
	{

	};

	class ITerrainGenerator
	{

	};

	struct VisibleChunkMeshes
	{
		std::tuple<GPUBufferHandle, GPUBufferHandle> gpuBuffers;
		std::unordered_set<AllocatedChunkSlot>& meshes;
	};

	class TerrainService
	{
	public:
		void Initialize(const TerrainGenerationDesc& desc, Graphics::IGraphicsBackend* backend);
		void Shutdown(Graphics::IGraphicsBackend* backend);
		uint32_t GetBlock(int x, int y, int z);
		void SetBlock(int x, int y, int z, uint32_t value);
		ChunkData* GetChunkForRead(int chunkX, int chunkY, int chunkZ);
		void Update(float dt, Point3 chunkOrigin);
		void UploadBuiltChunks(Graphics::ICommandList* cmd);
		// free stage buffers, destroyed chunk buffers
		void GarbageCollect(Graphics::IGraphicsBackend* backend);

		VisibleChunkMeshes GetVisibleChunkMeshes()
		{
			return VisibleChunkMeshes {
				m_terrainMeshBuilder.GetGpuBuffers(),
				m_terrainUpdateScheduler.GetCurrentVisibleChunks(),
			};
		}
	private:
		TerrainData m_terrainData;
		std::unique_ptr<ITerrainGenerator> m_terrainGenerator;
		TerrainMeshBuilder m_terrainMeshBuilder;
		TerrainUpdateScheduler m_terrainUpdateScheduler;
	};
}
