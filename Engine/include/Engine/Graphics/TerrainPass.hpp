#pragma once
#include "IPass.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

namespace OneGame::Engine::Graphics
{

	class TerrainPass : public IPass
	{
		struct UBO
		{
			math::mat4 mvp;
		};
	public:
		TerrainPass() : orientation(math::identity_quat())
		{
		}

		void Initialize(IGraphicsBackend* backend, InitContext& ctxt) override;
		void Shutdown(IGraphicsBackend* backend) override;
		void Prepare(PrepareContext& context) override;
		void Draw(DrawContext& context) override;
		void SetPalette(ColorRGBA8 colors[16]);

	private:
		GPUBufferHandle colorPaletteGpuBuffer;
		void* colorPaletteStagingBuffer;
		bool isColorPaletteDirty = true;

		Mesh terrainMesh;
		std::vector<PTerrainMesh> activeChunkSlots;
		std::vector<UBO> ubos;

		GPUPipelineHandle pipelineHandle;
		GPUBindingGroupLayoutHandle bindingGroupLayout;
		GPUBindingGroupHandle bindingGroup;
		GPUTextureHandle blockTexture;

		math::quat orientation;
	};
}
