#pragma once

#include <unordered_map>
#include <string>
#include <entt/entt.hpp>
#include <queue>
#include "Engine/ObjectType.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"

namespace OneGame::Engine
{

	struct CPUTexture
	{
		int width;
		int height;
		Graphics::StagingAllocation data;
	};

	enum class ShaderType
	{
		VulkanBinary
	};

	struct CPUShader
	{
		std::vector<char> data;
		ShaderType type;
	};

	template<typename Handle>
	using AssetLoader = std::function<Handle(const std::string&)>;

	template<typename Handle>
	class AssetStorage
	{
	public:
		void SetLoader(AssetLoader<Handle> loader)
		{
			m_loader = std::move(loader);
		}

		Handle Load(const std::string_view& id)
		{
			auto strId = std::string(id);
			if (auto it = m_cache.find(std::string(strId)); it != m_cache.end())
				return it->second;

			if (!m_loader)
				return m_default;

			auto handle = m_loader(strId);

			if (!handle.IsValid())
				return m_default;

			m_cache.emplace(id, handle);
			return handle;
		}

		void SetDefault(Handle h) { m_default = h; }

	private:
		std::unordered_map<std::string, Handle> m_cache;
		AssetLoader<Handle> m_loader;
		Handle m_default{};
	};

	bool TryLoadPNG(std::vector<char> data, int& width, int& height, void* result);
	bool TryLoadBlob(const std::string_view& id, std::vector<char>&);

	class AssetManager
	{
	public:
		void StageUpload(const Graphics::IGraphicsBackend* backend, Graphics::RingStagingBuffer& stagingBuffer, Graphics::ICommandList* transferCmd)
		{
			auto fidx = backend->CurrentFrameIndex();
			while (!m_stagingBuffersToFree[fidx].empty())
			{
				auto& buffer = m_stagingBuffersToFree[fidx].back();
				stagingBuffer.Free(buffer.offset, buffer.size);
				m_stagingBuffersToFree[fidx].pop();
			}
			while (!m_texturesToUpload.empty())
			{
				auto& [gpuHandle, cpuTex] = m_texturesToUpload.back();
				transferCmd->TextureBarrier(gpuHandle, Graphics::TextureState::TransferDst);
				transferCmd->CopyBufferToTexture(stagingBuffer.GetBuffer(), gpuHandle, cpuTex.data.offset);
				transferCmd->TextureBarrier(gpuHandle, Graphics::TextureState::ShaderRead);

				m_stagingBuffersToFree[fidx].push(cpuTex.data);
				m_texturesToUpload.pop();
			}
		}

		bool LoadTexture(const std::string_view& id, Graphics::RingStagingBuffer& stagingBuffer, Graphics::IGraphicsBackend* backend, GPUTextureHandle& outTexture)
		{
			auto it = m_textures.find(std::string(id));
			if (it != m_textures.end())
			{
				outTexture = it->second;
				return true;
			}

			std::vector<char> blob;
			assert(TryLoadBlob(id, blob));
			CPUTexture result;
			TryLoadPNG(blob, result.width, result.height, nullptr);
			result.data = stagingBuffer.Allocate(result.width * result.height * sizeof(char) * 4 * 2);
			TryLoadPNG(blob, result.width, result.height, result.data.cpuPtr);

			Graphics::TextureDesc texDesc{};
			texDesc.width = result.width;
			texDesc.height = result.height;
			texDesc.format = Graphics::TextureFormat::RGBA8Unorm;
			texDesc.usage = Graphics::TextureUsage::TransferDst | Graphics::TextureUsage::Sampled;
			auto texture = backend->CreateTexture(texDesc);

			m_textures.emplace(id, texture);
			m_texturesToUpload.push({ texture, result });
			outTexture = texture;
			return true;
		}

		bool LoadShader(const std::string_view& id, std::vector<char>& outShader)
		{
			return TryLoadBlob(id, outShader);
		}

	private:
		std::unordered_map<std::string, GPUTextureHandle> m_textures;
		std::queue<std::tuple<GPUTextureHandle, CPUTexture>> m_texturesToUpload;
		std::array<std::queue<Graphics::StagingAllocation>, 4> m_stagingBuffersToFree;
	};
}
