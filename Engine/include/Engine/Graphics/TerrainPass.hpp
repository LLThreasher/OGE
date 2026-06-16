#pragma once
#include "IPass.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"

namespace OneGame::Engine::Graphics
{

	class TerrainPass : public IPass
	{
	public:
		void Initialize(IGraphicsBackend* backend, InitContext& ctxt) override;
		void Shutdown(IGraphicsBackend* backend) override;
		void Prepare(entt::registry* world) override;
		void Draw(DrawContext& context) override;
		void SetPalette(ColorRGBA8 colors[16]);

	private:
		std::array<ColorRGBA8, 16> colorPalette;

		GPUPipelineHandle pipelineHandle;
		GPUBindingGroupLayoutHandle bindingGroupLayout;
		GPUBindingGroupHandle bindingGroup;
		GPUTextureHandle blockTexture;
	};
}
