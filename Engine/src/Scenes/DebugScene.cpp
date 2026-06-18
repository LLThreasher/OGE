#include "Engine/Scenes/DebugScene.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"

namespace OneGame::Engine
{
	void DebugScene::Initialize(AppInitContext& context)
	{
		auto& world = context.world;
		auto chunkEntity = world.create();
		world.emplace<Terrain::ActiveChunkTag>(chunkEntity);
		Terrain::ChunkMesh cm{ 0, 0, 0, { 0, 36 } };
		world.emplace<Terrain::ChunkMesh>(chunkEntity, cm);

		Terrain::TerrainGenerationDesc desc{};
		terrain.Initialize(desc, &context.assets);
	}

	void DebugScene::Enter(AppContext& context)
	{

	}

	void DebugScene::Update(AppContext& context, float dt)
	{

	}

	void DebugScene::Exit(AppContext& context)
	{

	}
}