#pragma once

#include <cinttypes>

namespace OneGame::Engine
{

	enum ObjectType : uint32_t
	{
		GPUBuffer,
		GPUTexture,
		GPUPipeline,
		GPUBindingGroupLayout,
		GPUBindingGroup,
		GPUFence,
		GPUQueryPool,
		GPURenderPass,
		GPUFrameBuffer,

		Texture,
		MaterialFormat,
		Material,
		Pipeline,
		Mesh,
	};


	template<ObjectType Tag>
	struct ResourceHandle
	{
		uint32_t index = 0;
		uint32_t generation = 0;

		bool IsValid() const { return index != 0; }

		static ResourceHandle<Tag> InvalidHandle()
		{
			return ResourceHandle<Tag>{ 0, 0 };
		}
	};
}
