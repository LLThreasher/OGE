#include "Engine/Scenes/DebugScene.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Random.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

namespace OneGame::Engine
{
	void DebugScene::Initialize(AppInitContext& context)
	{
		Terrain::TerrainGenerationDesc desc{};
		terrain.Initialize(desc);
	}

	void DebugScene::Enter(AppContext& context)
	{
		auto& world = context.world;
		auto chunkEntity = world.create();
		world.emplace<Terrain::ActiveChunkTag>(chunkEntity);
		Terrain::ChunkMesh cm{ 0, 0, 0, { 0, 36 } };
		world.emplace<Terrain::ChunkMesh>(chunkEntity, cm);
		
		chunkMesh = context.assets.LoadMesh(chunk_zero_vertices, chunk_zero_indices);
	}

	void DebugScene2::Initialize(AppInitContext& context)
	{
		using namespace ECS;
		gameWorld.Register<SubsystemCamera>();
		gameWorld.Register<SubsystemDebugInfo>();
		gameWorld.Initialize(context);
	}

	void DebugScene2::Enter(AppContext& context)
	{
		auto handle = terrainData.chunkData.Create();
		auto chunk = terrainData.chunkData.Get(handle);
		chunk->Coords = { 0, 0, 0 };
		chunk->data[0] = 256;
		for (size_t i = 0; i < 200; ++i)
		{
			auto x = Random::RandInt(0, 15);
			auto y = Random::RandInt(0, 15);
			auto z = Random::RandInt(0, 15);
			assert(x < 16);
			assert(y < 16);
			assert(z < 16);

			auto idx = Terrain::GetBlockIndex(x, y, z);
			assert(idx < 16 * 16 * 16);
			chunk->data[idx] = 256;
		}

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
		terrainData.coordToChunks[{0, 0, 0}] = handle;
		{
			auto nhandle = terrainData.chunkData.Create();
			terrainData.chunkData.Get(nhandle)->Coords = { 1, 0, 0 };
			terrainData.coordToChunks[{1, 0, 0}] = nhandle;
		}
		{
			auto nhandle = terrainData.chunkData.Create();
			terrainData.chunkData.Get(nhandle)->Coords = { -1, 0, 0 };
			terrainData.coordToChunks[{-1, 0, 0}] = nhandle;
		}
		{
			auto nhandle = terrainData.chunkData.Create();
			terrainData.chunkData.Get(nhandle)->Coords = { 0, 1, 0 };
			terrainData.coordToChunks[{0, 1, 0}] = nhandle;
		}
		{
			auto nhandle = terrainData.chunkData.Create();
			terrainData.chunkData.Get(nhandle)->Coords = { 0, 0, 1 };
			terrainData.coordToChunks[{0, 0, 1}] = nhandle;
		}
		{
			auto nhandle = terrainData.chunkData.Create();
			terrainData.chunkData.Get(nhandle)->Coords = { 0, 0, -1 };
			terrainData.coordToChunks[{0, 0, -1}] = nhandle;
		}
		terrainData.buildMeshQueue.push(handle);
		meshBuilder.BuildChunkMeshes(192 * 1024 * 1024);

		auto [chunkHandle, builtMeshHandle] = std::move(terrainData.uploadMeshQueue.front());
		auto builtMesh = terrainData.builtChunkMeshes.Get(builtMeshHandle);

#ifdef USE_TERRAIN_MESH_V2
		meshBuilder.AllocateTerrainMesh(context.assets.m_backend);
		context.assets.m_streamingManager->UploadBuffer<UploadType::Immediate, BufferUsage::Storage>(builtMesh->quads, terrainData.terrainMesh);
		testSlot = { 0, (uint32_t)builtMesh->quads.size() };

		//Terrain::TexturedQuad quad{ 0u, 0u, 0u, 1u, 4u, COLOR_WHITE, { 0xF, 0xF, 0xF, 0xF }, { 0, 0, 0, 0 } };
		//std::vector<Terrain::TexturedQuad> data;
		//data.push_back(quad);
		//context.assets->UpdateBuffer(data, 0, terrainMesh);
		//testSlot = { 0, 1 };
#else
		Mesh terrainMesh;
		assert(context.assets->LoadMesh("terrainMesh", terrainMesh));
		context.assets->UpdateMesh(builtMesh->vertices, builtMesh->indices, terrainMesh);
		testSlot = { 0, (uint32_t)builtMesh->indices.size() };
#endif
	}

	void DebugScene2::Update(AppContext& context, float dt)
	{
		auto& world = context.world;
		auto chunkEntity = world.create();
#ifdef USE_TERRAIN_MESH_V2
		world.emplace<Graphics::PTerrainMesh2>(chunkEntity, testSlot.chunkSlot, testSlot.indexCount, 0, 0, 0);
#else
		world.emplace<Graphics::PTerrainMesh>(chunkEntity, testSlot.chunkSlot, testSlot.indexCount, 0, 0, 0);
#endif

		if (wrappingEnabled)
			gameWorld.Update(context, dt);

		if (context.input.IsKeyDown(KeyCode::KY_G))
		{
			SceneAction a{};
			a.type = SceneActionType::SetMouseWarpping;
			a.setMouseWarpping.enabled = true;
			context.outSceneActions.emplace_back(a);
			wrappingEnabled = true;
		}
		else if (context.input.IsKeyDown(KeyCode::KY_ESCAPE))
		{
			SceneAction a{};
			a.type = SceneActionType::SetMouseWarpping;
			a.setMouseWarpping.enabled = false;
			context.outSceneActions.emplace_back(a);
			wrappingEnabled = false;
		}
	}
}
