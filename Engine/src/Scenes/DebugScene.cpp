#include "Engine/Scenes/DebugScene.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Random.hpp"

namespace OneGame::Engine
{
	void DebugScene::Initialize(AppInitContext& context)
	{
		Terrain::TerrainGenerationDesc desc{};
		terrain.Initialize(desc, &context.assets);
	}

	void DebugScene::Enter(AppContext& context)
	{
		auto& world = context.world;
		auto chunkEntity = world.create();
		world.emplace<Terrain::ActiveChunkTag>(chunkEntity);
		Terrain::ChunkMesh cm{ 0, 0, 0, { 0, 36 } };
		world.emplace<Terrain::ChunkMesh>(chunkEntity, cm);

		Mesh terrainMesh;
		assert(context.assets->LoadMesh("terrainMesh", terrainMesh));
		context.assets->UpdateMesh(chunk_zero_vertices, chunk_zero_indices, 0, terrainMesh);
	}

	void DebugScene2::Initialize(AppInitContext& context)
	{
		using namespace ECS;
		gameWorld.Register<SubsystemCamera>();
		gameWorld.Register<SubsystemDebugInfo>();
		gameWorld.Initialize(context);
		meshBuilder.Initialize(&context.assets);
	}

	void DebugScene2::Enter(AppContext& context)
	{
		auto handle = terrainData.chunkData.Create();
		auto chunk = terrainData.chunkData.Get(handle);
		chunk->Coords = { 0, 0, 0 };
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

		Mesh terrainMesh;
		assert(context.assets->LoadMesh("terrainMesh", terrainMesh));
		context.assets->UpdateMesh(builtMesh->vertices, builtMesh->indices, 0, terrainMesh);

		testSlot = { 0, (uint32_t)builtMesh->indices.size() };
	}

	void DebugScene2::Update(AppContext& context, float dt)
	{
		auto& world = context.world;
		auto chunkEntity = world.create();
		world.emplace<Terrain::ActiveChunkTag>(chunkEntity);
		Terrain::ChunkMesh cm{ 0, 0, 0, testSlot };
		world.emplace<Terrain::ChunkMesh>(chunkEntity, cm);
		gameWorld.Update(context, dt);
	}
}
