#include "Engine/Scenes/DebugScene.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Random.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
	void DebugScene::Initialize(AppContext& context)
	{
		Terrain::TerrainGenerationDesc desc{};
		terrain.Initialize(desc);
	}

	void DebugScene::Enter(AppContext& context)
	{
		chunkMesh = context.CreateAssetBundle().LoadMesh(chunk_zero_vertices, chunk_zero_indices);
	}

	void DebugScene2::Initialize(AppContext& context)
	{
		using namespace ECS;
		gameWorld.Register<SubsystemCamera>();
		gameWorld.Register<SubsystemDebugInfo>();
		gameWorld.Initialize(context);
	}

	void DebugScene2::Enter(AppContext& context)
	{
		using BufferUsage = Graphics::BufferUsage;
		//for (size_t x = 0; x < 16; ++x)
		//{
		//	for (size_t y = 0; y < 16; ++y)
		//	{
		//		for (size_t z = 0; z < 16; ++z)
		//		{
		//			if (x == 1 || x == 2 || x == 3 || x == 4)
		//			{
		//				auto idx = Terrain::GetBlockIndex(x, y, z);
		//				chunk->data[idx] = 256;
		//			}
		//		}
		//	}
		//}


		//auto handle = terrainData.chunkData.Create();
		//auto chunk = terrainData.chunkData.Get(handle);
		//chunk->Coords = { cx, cy, cz };
		//terrainData.coordToChunks[{cx, cy, cz}] = handle;
		//for (size_t i = 0; i < 200; ++i)
		//{
		//	auto x = Random::RandInt(0, 15);
		//	auto y = Random::RandInt(0, 15);
		//	auto z = Random::RandInt(0, 15);
		//	assert(x < 16);
		//	assert(y < 16);
		//	assert(z < 16);

		//	auto idx = Terrain::GetBlockIndex(x, y, z);
		//	assert(idx < 16 * 16 * 16);
		//	chunk->data[idx] = 256;
		//}

		static int chunkCount = 1024;
		Graphics::ChunkAllocator gpuBufferAllocator(chunkCount);

		auto generate_terrain = [](int x, int y, int z) {
			//if (z > 14)
			//	return 0;
			if (((x + y + z) % 2) == 0) {
				return 256;
			}
			return 0;
			};

		LOG_DEBUG("start generate terrain");
		for (int cx = -10; cx < 10; ++cx)
		{
			for (int cz = -10; cz < 10; ++cz)
			{
				for (int cy = 0; cy < 5; ++cy)
				{
					auto handle = terrainData.chunkData.Create();
					auto chunk = terrainData.chunkData.Get(handle);
					chunk->Coords = { cx, cy, cz };
					terrainData.coordToChunks[{cx, cy, cz}] = handle;

					for (size_t z = 0; z < 16; ++z)
					{
						for (size_t y = 0; y < 16; ++y)
						{
							for (size_t x = 0; x < 16; ++x)
							{
								chunk->SetBlock(x, y, z, generate_terrain(x, y, z));
							}
						}
					}
				}
			}
		}
		LOG_DEBUG("end generate terrain");


		LOG_DEBUG("start generate mesh");
		for (int cx = -3; cx < 3; ++cx)
		{
			for (int cz = -3; cz < 3; ++cz)
			{
				for (int cy = 0; cy < 4; ++cy)
				{
					auto handle = terrainData.coordToChunks[{cx, cy, cz}];
					assert(handle.IsValid());
					auto coord = terrainData.chunkData.Get(handle)->Coords;
					LOG_DEBUG("queuing chunk ({}, {}, {}) with ({}, {}, {})", cx, cy, cz, coord.x, coord.y, coord.z);
					terrainData.buildMeshQueue.push(handle);
				}
			}
		}
		meshBuilder.BuildChunkMeshes(192 * 1024 * 1024);
		LOG_DEBUG("end generate mesh");

#ifdef USE_TERRAIN_MESH_V2
		terrainData.terrainMesh = context.backend.AllocateGPUBuffer<BufferUsage::Storage>(chunkCount * 4 * Terrain::CHUNK_STORE_BYTE_SIZE);
#else
		m_terrain.terrainMesh = {};
		m_terrain.terrainMesh.vertexBuffer = backend.AllocateGPUBuffer<BufferUsage::Vertex>(MAX_VISIBLE_CHUNK_NUM * CHUNK_VERTEX_BYTE_SIZE);
		m_terrain.terrainMesh.indexBuffer = backend.AllocateGPUBuffer<BufferUsage::Index>(MAX_VISIBLE_CHUNK_NUM * CHUNK_VERTEX_BYTE_SIZE);
#endif
		meshBuilder.AllocateTerrainMesh(context.backend);
		auto bundle = context.CreateAssetBundle();
#ifdef USE_TERRAIN_MESH_V2
		context.renderer.EnableTerrainPass2(context.backend, bundle, terrainData.terrainMesh);

		int active_count = chunkCount - 23;
		LOG_INFO("total used size: {} {}", active_count, active_count * Terrain::CHUNK_STORE_BYTE_SIZE * 4);
		int currentSlot = 0;
		auto event = context.streamingManager.CreateResourceBundle([&] {
			isTerrainReady = true;
			});
		while (!terrainData.uploadMeshQueue.empty() && currentSlot < active_count)
		{
			auto [chunkHandle, builtMeshHandle] = std::move(terrainData.uploadMeshQueue.front());
			terrainData.uploadMeshQueue.pop();
			auto builtMesh = terrainData.builtChunkMeshes.Get(builtMeshHandle);
			auto chunkCoord = terrainData.chunkData.Get(chunkHandle)->Coords;
			context.streamingManager.UploadBuffer<UploadType::Async, BufferUsage::Storage>(builtMesh->quads, terrainData.terrainMesh, currentSlot * Terrain::CHUNK_STORE_BYTE_SIZE, event);
			testSlots.push_back(Graphics::PTerrainMesh{ currentSlot * Terrain::CHUNK_STORE_BYTE_SIZE, static_cast<uint32_t>(builtMesh->quads.size()) * 6, chunkCoord.x, chunkCoord.y, chunkCoord.z });
			currentSlot += 4;
			assert(builtMesh->quads.size() * sizeof(Terrain::TexturedQuad) <= Terrain::CHUNK_STORE_BYTE_SIZE * 4);
			LOG_DEBUG("looking at chunk ({}, {}, {}), data size: {} kb", chunkCoord.x, chunkCoord.y, chunkCoord.z, (float)(builtMesh->quads.size() * sizeof(Terrain::TexturedQuad)) / 1024.f);
		}
		LOG_DEBUG("used slots {}", currentSlot);
#else
		context.renderer.EnableTerrainPass(context.backend, bundle, terrainData.terrainMesh);
		int currentSlot = 0;
		while (!terrainData.uploadMeshQueue.empty() && currentSlot < 100)
		{
			auto [chunkHandle, builtMeshHandle] = std::move(terrainData.uploadMeshQueue.front());
			terrainData.uploadMeshQueue.pop();
			auto builtMesh = terrainData.builtChunkMeshes.Get(builtMeshHandle);
			auto chunkCoord = terrainData.chunkData.Get(chunkHandle)->Coords;
			if (chunkCoord.x > 2 || chunkCoord.z > 2 || chunkCoord.x < -2 || chunkCoord.z < -2)
				continue;
			Mesh m = terrainData.terrainMesh;
			m.vOffset = currentSlot * Terrain::CHUNK_VERTEX_BYTE_SIZE;
			m.iOffset = currentSlot * Terrain::CHUNK_INDEX_BYTE_SIZE;
			bundle.UploadMesh(builtMesh->vertices, builtMesh->indices, m);
			testSlots.push_back(Graphics::PTerrainMesh{ (uint32_t)currentSlot, (uint32_t)builtMesh->indices.size(), chunkCoord.x, chunkCoord.y, chunkCoord.z });
			currentSlot += 2;
			LOG_DEBUG("looking at chunk ({}, {}, {}), vert size: {} kb, index size: {} kb", chunkCoord.x, chunkCoord.y, chunkCoord.z, (float)(builtMesh->vertices.size() * sizeof(Terrain::Vertex)) / 1024.f, (builtMesh->indices.size() * sizeof(uint16_t)) / 1024.f);
		}
		LOG_DEBUG("used slots {}", currentSlot);
#endif
	}

	void DebugScene2::Exit(AppContext& context)
	{
#ifdef USE_TERRAIN_MESH_V2
		context.renderer.DisableTerrainPass2(context.backend);
#else
		context.renderer.DisableTerrainPass(context.backend);
#endif
	}

	void DebugScene2::Update(AppContext& context, FrameContext& frame)
	{
		auto& world = frame.presentationWorld;
		if (isTerrainReady)
		{
			for (auto& slot : testSlots)
			{
				auto chunkEntity = world.create();
				world.emplace<Graphics::PTerrainMesh>(chunkEntity, slot);
			}
		}

		if (wrappingEnabled)
			gameWorld.Update(context, frame);

		if (frame.input.IsKeyDown(KeyCode::KY_G) || firstFrame)
		{
			SceneAction a{};
			a.type = SceneActionType::SetMouseWarpping;
			a.setMouseWarpping.enabled = true;
			frame.outSceneActions.emplace_back(a);
			wrappingEnabled = true;
			firstFrame = false;
		}
		else if (frame.input.IsKeyDown(KeyCode::KY_ESCAPE))
		{
			SceneAction a{};
			a.type = SceneActionType::SetMouseWarpping;
			a.setMouseWarpping.enabled = false;
			frame.outSceneActions.emplace_back(a);
			wrappingEnabled = false;
		}
	}
}
