#pragma once

#include <entt/entt.hpp>
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/UniformArena.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/AssetManager.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::Graphics
{
	struct InitContext {
		AssetManager& assets;
		UniformArena& uniformArena;
		RingStagingBuffer& ringStagingBuffer;
	};

	struct DrawContext
	{
		float deltaTime;
		UniformArena* uniformArena;
		ICommandList* drawCmd;
		const IGraphicsBackend* backend;
		ICommandList* transferCmd;
	};
	
	struct AppContext;

	class IPass
	{
	public:
		virtual ~IPass() = default;

		virtual void Initialize(IGraphicsBackend* backend, InitContext& ctx) = 0;
		virtual void Shutdown(IGraphicsBackend* backend) = 0;
		virtual void Prepare(entt::registry* world) = 0;
		virtual void Draw(DrawContext& context) = 0;
	};
}
