#pragma once

#include <string>
#include "Engine/GameAppState.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/ECS/GameWorld.hpp"

#include "IScene.hpp"

namespace OneGame::Engine
{
	const std::vector<Terrain::Vertex> chunk_zero_vertices =
	{
		// FRONT (+Z)
		{ 0, 0, 1, 0, 0xF, 0, 0,  0  },
		{ 1, 0, 1, 0, 0xF, 0, 15, 0  },
		{ 1, 1, 1, 0, 0xF, 0, 15, 15 },
		{ 0, 1, 1, 0, 0xF, 0, 0,  15 },

		// BACK (-Z)
		{ 1, 0, 0, 0, 0xF, 0, 0,  0  },
		{ 0, 0, 0, 0, 0xF, 0, 15, 0  },
		{ 0, 1, 0, 0, 0xF, 0, 15, 15 },
		{ 1, 1, 0, 0, 0xF, 0, 0,  15 },

		// LEFT (-X)
		{ 0, 0, 0, 0, 0xF, 0, 0,  0  },
		{ 0, 0, 1, 0, 0xF, 0, 15, 0  },
		{ 0, 1, 1, 0, 0xF, 0, 15, 15 },
		{ 0, 1, 0, 0, 0xF, 0, 0,  15 },

		// RIGHT (+X)
		{ 1, 0, 1, 0, 0xF, 0, 0,  0  },
		{ 1, 0, 0, 0, 0xF, 0, 15, 0  },
		{ 1, 1, 0, 0, 0xF, 0, 15, 15 },
		{ 1, 1, 1, 0, 0xF, 0, 0,  15 },

		// TOP (+Y)
		{ 0, 1, 1, 0, 0xF, 0, 0,  0  },
		{ 1, 1, 1, 0, 0xF, 0, 15, 0  },
		{ 1, 1, 0, 0, 0xF, 0, 15, 15 },
		{ 0, 1, 0, 0, 0xF, 0, 0,  15 },

		// BOTTOM (-Y)
		{ 0, 0, 0, 0, 0xF, 0, 0,  0  },
		{ 1, 0, 0, 0, 0xF, 0, 15, 0  },
		{ 1, 0, 1, 0, 0xF, 0, 15, 15 },
		{ 0, 0, 1, 0, 0xF, 0, 0,  15 },
	};

	const std::vector<uint16_t> chunk_zero_indices =
	{
		0,1,2, 2,3,0,        // front
		4,5,6, 6,7,4,        // back
		8,9,10, 10,11,8,     // left
		12,13,14, 14,15,12,  // right
		16,17,18, 18,19,16,  // top
		20,21,22, 22,23,20   // bottom
	};

	class EmptyScene : public IScene
	{
	public:
		virtual void Initialize(AppInitContext& context) override
		{
		}
		virtual void Enter(AppContext& context) override
		{
		}
		virtual void Update(AppContext& context, float dt) override
		{
		}
		virtual void Exit(AppContext& context) override
		{
		}
	};

	class DebugScene : public EmptyScene
	{
	public:
		virtual void Initialize(AppInitContext& context) override;
		virtual void Enter(AppContext& context) override;
	protected:
		Terrain::TerrainService terrain;
	};

	class DebugScene2 : public EmptyScene
	{
	public:
		DebugScene2() : meshBuilder(terrainData)
		{
		}
		virtual void Initialize(AppInitContext& context) override;
		virtual void Enter(AppContext& context) override;
		virtual void Update(AppContext& context, float dt) override;
	protected:
		Terrain::ChunkSlot testSlot;
		Terrain::TerrainData terrainData;
		Terrain::TerrainMeshBuilder meshBuilder;

		ECS::GameWorld gameWorld;

		bool wrappingEnabled = false;
	};
}
