#pragma once

#include <string>
#include <entt/entt.hpp>
#include "Engine/ObjectType.hpp"
#include "Engine/Graphics/IPass.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::Graphics
{
	constexpr size_t NUM_DEBUG_VERTICES = 2048;
	constexpr size_t NUM_DEBUG_INDICES = NUM_DEBUG_VERTICES / 4 * 6;

	class DebugInfoPass : public IPass
	{
		struct PushConstant
		{
			math::mat2 transform;
			math::vec2 offset;
		};

		struct Vertex
		{
			math::vec3 pos;
			ColorRGBA8 color;
		};

	public:
		void Initialize(IGraphicsBackend* backend, InitContext& ctx) override;
		void Shutdown(IGraphicsBackend* backend) override;
		void Prepare(PrepareContext& context) override;
		void Draw(DrawContext& context) override;
	private:
		Vertex vertices[NUM_DEBUG_VERTICES];
		uint16_t indices[NUM_DEBUG_INDICES];
		size_t numQuads;
		PushConstant pushConstant;

		GPUPipelineHandle pipeline;
		GPUBindingGroupLayoutHandle bindingGroupLayout;
		GPUBufferHandle vertexBuffer;
		GPUBufferHandle indexBuffer;
		GPUBindingGroupHandle bindingGroup;
	};

	class TestPass : public IPass
	{
		struct UBO
		{
			math::mat4 model;
			math::mat4 view;
			math::mat4 proj;
		};

		struct Vertex
		{
			math::vec3 pos;
			math::vec3 color;
			math::vec2 uv;
		};

	public:

		void Initialize(IGraphicsBackend* backend, InitContext& ctx) override;
		void Shutdown(IGraphicsBackend* backend) override;
		void Prepare(PrepareContext& context) override;
		void Draw(DrawContext& context) override;

	private:
		float m_Time = 0.0f;
		float angle = 0.0f;
		GPUPipelineHandle pipeline;
		GPUBindingGroupLayoutHandle bindingGroupLayout;
		GPUBindingGroupHandle bindingGroup;
		StagingAllocation textureBuffer;
		GPUTextureHandle texture;

		Mesh testCubeMesh;
	};
}
