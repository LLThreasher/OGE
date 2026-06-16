#pragma once

#include <string>
#include <entt/entt.hpp>
#include "Engine/ObjectType.hpp"
#include "Engine/Graphics/IPass.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::Graphics
{
	constexpr size_t NUM_DEBUG_VERTICES = 4096;
	constexpr size_t NUM_DEBUG_INDICES = NUM_DEBUG_VERTICES / 4 * 6;

	class DebugInfoPass : public IPass
	{
		struct Vertex
		{
			math::vec3 pos;
			ColorRGBA8 color;
		};

	public:
		// always drawn on top left corner
		struct ComponentDebugText
		{
			std::string text;
		};

		void Initialize(IGraphicsBackend* backend, InitContext& ctx) override;
		void Shutdown(IGraphicsBackend* backend) override;
		void Prepare(entt::registry* world) override;
		void Draw(DrawContext& context) override;
	private:
		Vertex vertices[NUM_DEBUG_VERTICES];
		uint16_t indices[NUM_DEBUG_INDICES];
		size_t numQuads;

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
		void Prepare(entt::registry* world) override;
		void Draw(DrawContext& context) override;

	private:
		float m_Time = 0.0f;
		float angle = 0.0f;
		GPUPipelineHandle pipeline;
		GPUBindingGroupLayoutHandle bindingGroupLayout;
		GPUBufferHandle vertexBuffer;
		GPUBufferHandle indexBuffer;
		GPUBindingGroupHandle bindingGroup;
		StagingAllocation textureBuffer;
		GPUTextureHandle texture;

		bool isFirstFrame = true;

		std::vector<Vertex> vertices =
		{
			// FRONT (+Z)
			{{-1,-1, 1}, {1,1,1}, {0,0}},
			{{ 1,-1, 1}, {1,1,1}, {1,0}},
			{{ 1, 1, 1}, {1,1,1}, {1,1}},
			{{-1, 1, 1}, {1,1,1}, {0,1}},

			// BACK (-Z)
			{{ 1,-1,-1}, {1,1,1}, {0,0}},
			{{-1,-1,-1}, {1,1,1}, {1,0}},
			{{-1, 1,-1}, {1,1,1}, {1,1}},
			{{ 1, 1,-1}, {1,1,1}, {0,1}},

			// LEFT (-X)
			{{-1,-1,-1}, {1,1,1}, {0,0}},
			{{-1,-1, 1}, {1,1,1}, {1,0}},
			{{-1, 1, 1}, {1,1,1}, {1,1}},
			{{-1, 1,-1}, {1,1,1}, {0,1}},

			// RIGHT (+X)
			{{ 1,-1, 1}, {1,1,1}, {0,0}},
			{{ 1,-1,-1}, {1,1,1}, {1,0}},
			{{ 1, 1,-1}, {1,1,1}, {1,1}},
			{{ 1, 1, 1}, {1,1,1}, {0,1}},

			// TOP (+Y)
			{{-1, 1, 1}, {1,1,1}, {0,0}},
			{{ 1, 1, 1}, {1,1,1}, {1,0}},
			{{ 1, 1,-1}, {1,1,1}, {1,1}},
			{{-1, 1,-1}, {1,1,1}, {0,1}},

			// BOTTOM (-Y)
			{{-1,-1,-1}, {1,1,1}, {0,0}},
			{{ 1,-1,-1}, {1,1,1}, {1,0}},
			{{ 1,-1, 1}, {1,1,1}, {1,1}},
			{{-1,-1, 1}, {1,1,1}, {0,1}},
		};

		std::vector<uint32_t> indices =
		{
			0,1,2, 2,3,0,        // front
			4,5,6, 6,7,4,        // back
			8,9,10, 10,11,8,     // left
			12,13,14, 14,15,12,  // right
			16,17,18, 18,19,16,  // top
			20,21,22, 22,23,20   // bottom
		};
	};
}
