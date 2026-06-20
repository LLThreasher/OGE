#include "Engine/Graphics/TerrainPass.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include <entt/entt.hpp>

namespace OneGame::Engine::Graphics
{
	using namespace Terrain;

	void TerrainPass::Initialize(IGraphicsBackend* backend, InitContext& ctxt)
	{
		BindingGroupLayoutDesc layout{};
		layout.textureCount = 1;
		layout.bufferCount = 2;
		layout.dynamicBufferMask = 1 << 1;
		bindingGroupLayout = backend->CreateBindingGroupLayout(layout);

		blockTexture = ctxt.assets.LoadTexture("blocks.png");
		{
			BindingGroupDesc desc{};
			desc.layout = bindingGroupLayout;
			desc.textures.push_back(blockTexture);
			desc.buffers.push_back({ colorPaletteGpuBuffer, sizeof(ColorRGBA8) * 16 });
			desc.buffers.push_back({ ctxt.uniformArena.GetBuffer(), sizeof(UBO) });
			bindingGroup = backend->CreateBindingGroup(desc);
		}

		{
			BufferDesc cpDesc{};
			cpDesc.memory = MemoryUsage::CPUToGPU;
			cpDesc.size = sizeof(ColorRGBA8) * 16;
			cpDesc.usage = BufferUsage::Uniform | BufferUsage::TransferDst;
			colorPaletteGpuBuffer = backend->CreateBuffer(cpDesc, &colorPaletteStagingBuffer);

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

			desc.bindingGroupLayouts.push_back(bindingGroupLayout);
			desc.writeDepth = true;
			desc.blending = false;
			desc.depthTest = true;
			desc.depthCompareOp = DepthCompareOp::Less;
			desc.cullMode = CullMode::Back;
			pipelineHandle = backend->CreateGraphicsPipeline(desc);
		}

		ColorRGBA8 colors[16] = {
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE,
			COLOR_WHITE
		};
		SetPalette(colors);
	}

	void TerrainPass::Shutdown(IGraphicsBackend* backend)
	{
		backend->DestroyBuffer(colorPaletteGpuBuffer);
		backend->DestroyPipeline(pipelineHandle);
		backend->DestroyBindingGroupLayout(bindingGroupLayout);
	}

	void TerrainPass::Draw(DrawContext& ctxt)
	{
		if (activeChunkSlots.size() == 0)
			return;

		ctxt.drawCmd->BindGraphicsPipeline(pipelineHandle);

		auto uniformMemory = ctxt.uniformArena->Allocate(sizeof(UBO) * ubos.size());
		std::memcpy(uniformMemory.cpuPtr, ubos.data(), sizeof(UBO) * ubos.size());

		for (uint32_t i = 0; i < ubos.size(); ++i)
		{
			uint32_t offset[1] = { static_cast<uint32_t>(uniformMemory.offset + i * sizeof(UBO)) };
			ctxt.drawCmd->BindBindingGroup(bindingGroup, 0, offset);
			ctxt.drawCmd->BindVertexBuffer(terrainMesh.vertexBuffer, activeChunkSlots[i].chunkSlot * Terrain::CHUNK_VERTEX_BYTE_SIZE);
			ctxt.drawCmd->BindIndexBuffer(terrainMesh.indexBuffer, activeChunkSlots[i].chunkSlot * Terrain::CHUNK_INDEX_BYTE_SIZE, IndexFormat::Uint16);
			ctxt.drawCmd->DrawIndexed(activeChunkSlots[i].indexCount);
		}
	}

	math::mat4 ComputeModelTransform(float time)
	{
		float w1 = 1.0f;
		float w2 = 1.41421356237f; // sqrt(2)

		math::mat4 rx = math::rotate(math::mat4(1.0f), w1 * time, math::vec3(1, 0, 0));
		math::mat4 ry = math::rotate(math::mat4(1.0f), w2 * time, math::vec3(0, 1, 0));

		return ry * rx;
	}

	void TerrainPass::Prepare(PrepareContext& context)
	{
		math::mat4 view;
		{
			auto views = context.world->view<PViewTransform>();
			if (views.empty())
			{
				view = math::lookAt(
					math::vec3(20, 20, 20),
					math::vec3(0, 0, 0),
					math::vec3(0, 1, 0));
			}
			else
			{
				view = context.world->get<PViewTransform>(views.front()).view;
			}
		}

		auto proj = math::get_perspective_rot(context.backend->SwapchainPretransform()) * math::perspective(
			math::radians(45.0f),
			context.backend->SwapchainAspect(),
			0.1f,
			100.f);

		activeChunkSlots.clear();
		ubos.clear();
		auto worldView = context.world->view<const PTerrainMesh>();
		for (auto [_, mesh] : worldView.each())
		{
			activeChunkSlots.push_back(mesh);

			auto model = math::translate(math::mat4(1.0f), math::vec3(mesh.chunkX, mesh.chunkY, mesh.chunkZ));

			UBO ubo
			{
				proj * view * model
			};
			ubos.push_back(ubo);
		}
	}

	void TerrainPass::SetPalette(ColorRGBA8 colors[16])
	{
		memcpy(colorPaletteStagingBuffer, colors, 16 * sizeof(ColorRGBA8));
	}

	void TerrainPass2::Initialize(IGraphicsBackend* backend, InitContext& ctxt)
	{
		BindingGroupLayoutDesc layout{};
		layout.textureCount = 1;
		layout.bufferCount = 2;
		layout.dynamicBufferMask = 0b11;
		layout.storageBufferMask = 0b01;
		bindingGroupLayout = backend->CreateBindingGroupLayout(layout);

		blockTexture = ctxt.assets.LoadTexture("blocks.png");

		{
			BindingGroupDesc desc{};
			desc.layout = bindingGroupLayout;
			desc.textures.push_back(blockTexture);
			desc.buffers.push_back({ storageBuffer, CHUNK_VERTEX_BYTE_SIZE });
			desc.buffers.push_back({ ctxt.uniformArena.GetBuffer(), sizeof(UBO) });
			bindingGroup = backend->CreateBindingGroup(desc);
		}

		{
			GraphicsPipelineDesc desc{};
			ctxt.assets.LoadShader("terrain2.vert.spv", desc.vertexShader);
			ctxt.assets.LoadShader("terrain2.frag.spv", desc.fragmentShader);

			desc.bindingGroupLayouts.push_back(bindingGroupLayout);
			desc.writeDepth = true;
			desc.blending = false;
			desc.depthTest = true;
			desc.depthCompareOp = DepthCompareOp::Less;
			desc.cullMode = CullMode::Back;
			pipelineHandle = backend->CreateGraphicsPipeline(desc);
		}
	}

	void TerrainPass2::Shutdown(IGraphicsBackend* backend)
	{
		backend->DestroyBuffer(storageBuffer);
		backend->DestroyPipeline(pipelineHandle);
		backend->DestroyBindingGroupLayout(bindingGroupLayout);
	}

	void TerrainPass2::Draw(DrawContext& ctxt)
	{
		if (activeChunkSlots.size() == 0)
			return;

		ctxt.drawCmd->BindGraphicsPipeline(pipelineHandle);

		auto uniformMemory = ctxt.uniformArena->Allocate(sizeof(UBO) * ubos.size());
		std::memcpy(uniformMemory.cpuPtr, ubos.data(), sizeof(UBO) * ubos.size());

		for (uint32_t i = 0; i < ubos.size(); ++i)
		{
			uint32_t offset[2] = { CHUNK_VERTEX_BYTE_SIZE * activeChunkSlots[i].chunkSlot, static_cast<uint32_t>(uniformMemory.offset + i * sizeof(UBO))};
			ctxt.drawCmd->BindBindingGroup(bindingGroup, 0, offset);
			ctxt.drawCmd->Draw(activeChunkSlots[i].faceCount * 6);
		}
	}

	void TerrainPass2::Prepare(PrepareContext& context)
	{
		math::mat4 view;
		{
			auto views = context.world->view<PViewTransform>();
			if (views.empty())
			{
				view = math::lookAt(
					math::vec3(3, 3, 3),
					math::vec3(0, 0, 0),
					math::vec3(0, 1, 0));
			}
			else
			{
				view = context.world->get<PViewTransform>(views.front()).view;
			}
		}

		auto proj = math::get_perspective_rot(context.backend->SwapchainPretransform()) * math::perspective(
			math::radians(45.0f),
			context.backend->SwapchainAspect(),
			0.1f,
			100.f);

		activeChunkSlots.clear();
		ubos.clear();
		auto worldView = context.world->view<const PTerrainMesh2>();
		for (auto [_, mesh] : worldView.each())
		{
			activeChunkSlots.push_back(mesh);

			auto model = math::translate(math::mat4(1.0f), math::vec3(mesh.chunkX, mesh.chunkY, mesh.chunkZ));

			UBO ubo
			{
				proj * view * model
			};
			ubos.push_back(ubo);
		}
	}
}
