#include "Engine/Terrain/Terrain.hpp"
#include "Engine/StreamingManager.hpp"

namespace OneGame::Engine::Terrain
{
	void TerrainUploader::UploadTerrain(TerrainData& terrain, StreamingManager& sm)
	{
		while (!terrain.uploadMeshQueue.empty())
		{
			auto [chunk, chunkMesh] = std::move(terrain.uploadMeshQueue.front());
			terrain.uploadMeshQueue.pop();
			ResourceBundleHandle res = sm.CreateResourceBundle([chunk, chunkMesh, &terrain]() {
				terrain.builtChunkMeshes.Destroy(chunkMesh);
				terrain.residentChunks.insert(chunk);
				});
			auto mesh = terrain.builtChunkMeshes.Get(chunkMesh);
			sm.UploadBuffer<UploadType::Async, Graphics::BufferUsage::Storage>(mesh->quads, terrain.terrainMesh);
		}
	}

	void TerrainUploader::SetMaxNumChunks(uint32_t maxNumChunks)
	{
		m_chunkAllocator.Initialize(maxNumChunks);
	}
}