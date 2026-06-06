#pragma once


namespace OneGame::Engine::Graphics
{
	template<ObjectType Tag>
	struct ResourceHandle;

	enum class GPUResourceType
	{
		Texture,
		MaterialFormat,
		Material,
		Pipeline,
		Mesh,
	};

	enum class GPUResourceState
	{
		Loading,
		Uploading,
		Alive,
		PendingDestruction,
		Destroyed,
	};

	template <GPUResourceType Type, typename T>
	struct GPUResource
	{
		T data;
		GPUResource state;
	};

	class GPUResourceManager
	{
		// This class will manage GPU resources like buffers, textures, pipelines, etc.
		// It will handle creation, destruction, and lifetime management of these resources.
		// It may also implement pooling and recycling of resources to minimize allocations.
	};
}