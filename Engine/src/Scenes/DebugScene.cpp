#include "Engine/Scenes/DebugScene.hpp"
#include "Engine/Terrain/Terrain.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Random.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/StreamingManager.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
	void DebugScene::Initialize(AppContext& context)
	{
		using namespace ECS;
		//Terrain::TerrainGenerationDesc desc{};
		//terrain.Initialize(desc);
		gameWorld.Register<SubsystemDebugInfo>();
		gameWorld.Register<SubsystemCamera>();
		gameWorld.Initialize(context);
	}

	void DebugScene::Enter(AppContext& context)
	{
		chunkMesh = context.backend.AllocateGPUBuffer<Graphics::BufferUsage::Storage>(16 * Terrain::CHUNK_STORE_BYTE_SIZE);
		context.streamingManager.UploadBuffer<UploadType::Immediate, Graphics::BufferUsage::Storage>(chunk_zero_quads, chunkMesh);
		auto bundle = context.CreateAssetBundle();
		context.renderer.EnableTerrainPass2(context.backend, bundle, chunkMesh);
	}

	void DebugScene::Exit(AppContext& context)
	{
		context.renderer.DisableTerrainPass2(context.backend);
	}

	void DebugScene::Update(AppContext& context, FrameContext& frame)
	{
		gameWorld.Update(context, frame);
		auto& world = frame.presentationWorld;
		auto chunkEntity = world.create();
		Graphics::PTerrainMesh tMesh = { 0u, 3u, 0, 0, 0 };
		world.emplace<Graphics::PTerrainMesh>(chunkEntity, tMesh);
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
		using namespace Terrain;
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

		static int chunkCount = 32;
		Graphics::ChunkAllocator gpuBufferAllocator;

		auto generate_terrain = [](int cx, int cy, int cz, int x, int y, int z) {
			//if (cx > -3 && cx < 2 && cy > 0 && cy < 3 && cz > -3 && cz < 2)
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
					auto handle = terrainData.chunks.AllocateChunk({ cx, cy, cz });
					auto chunk = terrainData.chunks.Get(handle);

					for (size_t z = 0; z < 16; ++z)
					{
						for (size_t y = 0; y < 16; ++y)
						{
							for (size_t x = 0; x < 16; ++x)
							{
								chunk->SetBlock(x, y, z, generate_terrain(cx, cy, cz, x, y, z));
							}
						}
					}
				}
			}
		}
		LOG_DEBUG("end generate terrain");


		LOG_DEBUG("start generate mesh");

		auto [handle, chunk] = terrainData.chunks.Get({ 0, 0, 0 });
		assert(handle.IsValid());
		auto coord = chunk->Coords;
		LOG_DEBUG("queuing chunk ({}, {}, {}) with ({}, {}, {})", 0, 0, 0, coord.x, coord.y, coord.z);
		terrainData.buildMeshQueue.push(handle);

		//for (int cx = -3; cx < 3; ++cx)
		//{
		//	for (int cz = -3; cz < 3; ++cz)
		//	{
		//		for (int cy = 0; cy < 4; ++cy)
		//		{
		//			auto [handle, chunk] = terrainData.chunks.Get({ cx, cy, cz });
		//			assert(handle.IsValid());
		//			auto coord = chunk->Coords;
		//			LOG_DEBUG("queuing chunk ({}, {}, {}) with ({}, {}, {})", cx, cy, cz, coord.x, coord.y, coord.z);
		//			terrainData.buildMeshQueue.push(handle);
		//		}
		//	}
		//}
		BlockRegistry blocks;
		meshBuilder.SetVertexBudget(192 * 1024 * 1024);
		meshBuilder.BuildChunkMeshes(terrainData, blocks);
		LOG_DEBUG("end generate mesh");

		terrainData.terrainMesh = context.backend.AllocateGPUBuffer<BufferUsage::Storage>(chunkCount * 4 * Terrain::CHUNK_STORE_BYTE_SIZE);
		auto bundle = context.CreateAssetBundle();
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
			auto chunkCoord = terrainData.chunks.Get(chunkHandle)->Coords;
			context.streamingManager.UploadBuffer<UploadType::Async, BufferUsage::Storage>(builtMesh->quads, terrainData.terrainMesh, currentSlot * Terrain::CHUNK_STORE_BYTE_SIZE, event);
			testSlots.push_back(Graphics::PTerrainMesh{ currentSlot * Terrain::CHUNK_STORE_BYTE_SIZE, static_cast<uint32_t>(builtMesh->quads.size()) * 6, chunkCoord.x, chunkCoord.y, chunkCoord.z });
			currentSlot += 4;
			assert(builtMesh->quads.size() * sizeof(Terrain::TexturedQuad) <= Terrain::CHUNK_STORE_BYTE_SIZE * 4);
			LOG_DEBUG("looking at chunk ({}, {}, {}), data size: {} kb", chunkCoord.x, chunkCoord.y, chunkCoord.z, (float)(builtMesh->quads.size() * sizeof(Terrain::TexturedQuad)) / 1024.f);
		}
		LOG_DEBUG("used slots {}", currentSlot);
	}

	void DebugScene2::Exit(AppContext& context)
	{
		context.renderer.DisableTerrainPass2(context.backend);
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
