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
		AssetBundleWriter<UploadType::Immediate>& assets;
		UniformArena& uniformArena;
	};

	struct DrawContext
	{
		const IGraphicsBackend* backend;
		float deltaTime;
		UniformArena* uniformArena;
		ICommandList* drawCmd;
		ICommandList* transferCmd;
	};

	struct PrepareContext
	{
		const IGraphicsBackend* backend;
		float deltaTime;
		entt::registry* world;
	};
	
	class IPass
	{
	public:
		virtual ~IPass() = default;

		virtual void Initialize(IGraphicsBackend* backend, InitContext& ctx) = 0;
		virtual void Shutdown(IGraphicsBackend* backend) = 0;
		virtual void Prepare(PrepareContext& context) = 0;
		virtual void Draw(DrawContext& context) = 0;
	};
}
