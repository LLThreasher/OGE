#pragma once

#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/UniformArena.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/AssetBundle.hpp"

namespace OneGame::Engine::Graphics
{
	struct InitContext
	{
		AssetPool& assets;
		UniformArena& uniformArena;
	};

	struct DrawContext
	{
		const IGraphicsBackend& backend;
		float deltaTime;
		UniformArena& uniformArena;
		ICommandList& drawCmd;
		ICommandList& transferCmd;
	};

	struct PrepareContext
	{
		const IGraphicsBackend& backend;
		float deltaTime;
		entt::registry& world;
	};
}
