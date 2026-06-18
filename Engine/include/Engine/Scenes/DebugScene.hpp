#pragma once

#include <string>
#include "Engine/GameAppState.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"

#include "IScene.hpp"

namespace OneGame::Engine
{
	class DebugScene : public IScene
	{
	public:
		void Initialize(AppInitContext& context) override;
		void Enter(AppContext& context) override;
		void Update(AppContext& context, float dt) override;
		void Exit(AppContext& context) override;
	private:
		Terrain::TerrainService terrain;
	};
}
