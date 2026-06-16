#include "Engine/Graphics/TerrainPass.hpp"

namespace OneGame::Engine::Graphics
{
	void TerrainPass::Initialize(IGraphicsBackend* backend, InitContext& ctxt)
	{
		BindingGroupLayoutDesc layout{};
		layout.textureCount = 1;
		layout.bufferCount = 1;
		layout.dynamicBufferMask = 1;
		bindingGroupLayout = backend->CreateBindingGroupLayout(layout);

		{
			//auto handle = ctxt.LoadTexture("blocks.png");
		}

		{
			BindingGroupDesc desc{};
			desc.layout = bindingGroupLayout;
			//desc.buffers.push_back()
		}

		{
			GraphicsPipelineDesc desc{};
			ctxt.assets.LoadShader("terrain.vert.spv", desc.vertexShader);
			ctxt.assets.LoadShader("terrain.frag.spv", desc.fragmentShader);
			// packed xyz
			desc.vertexLayout.push_back(VertexAttributeFormat::Uint16);
			// packed light and color
			desc.vertexLayout.push_back(VertexAttributeFormat::Uint8);
			// packed material id
			desc.vertexLayout.push_back(VertexAttributeFormat::Uint8);

			PushConstantRangeDesc cDesc{};
			cDesc.size = sizeof(uint32_t) * 16;
			cDesc.offset = 0;
			cDesc.stageFlags = ShaderStage::Fragment;
			desc.pushConstants.push_back(cDesc);

			desc.bindingGroupLayouts.push_back(bindingGroupLayout);
			desc.writeDepth = true;
			desc.blending = false;
			desc.depthTest = true;
			desc.depthCompareOp = DepthCompareOp::Less;
			desc.cullMode = CullMode::Back;
			pipelineHandle = backend->CreateGraphicsPipeline(desc);
		}

		for (size_t i = 0; i < 16; ++i)
		{
			colorPalette[i] = COLOR_WHITE;
		}
	}

	void TerrainPass::Shutdown(IGraphicsBackend* backend)
	{
		backend->DestroyPipeline(pipelineHandle);
		backend->DestroyBindingGroupLayout(bindingGroupLayout);
	}

	void TerrainPass::Draw(DrawContext& ctxt)
	{

	}

	void TerrainPass::Prepare(entt::registry* world)
	{

	}

	void TerrainPass::SetPalette(ColorRGBA8 colors[16])
	{

	}
}