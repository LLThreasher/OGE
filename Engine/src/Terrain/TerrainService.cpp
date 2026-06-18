#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Terrain/BlockManager.hpp"


namespace OneGame::Engine::Terrain
{
	void TerrainService::Initialize(const TerrainGenerationDesc& desc, AssetBundleWriter* assets)
	{
		m_terrainMeshBuilder.Initialize(assets);
	}
}