#pragma once

#include <vector>
#include <span>
#include <string>
#include <unordered_map>

#include "Engine/ObjectType.hpp"

namespace OneGame::Engine
{
class AssetManager;
class StreamingManager;
namespace Graphics
{
class IGraphicsBackend;
}

class AssetPool
{
   public:
    ~AssetPool() = default;

    bool Load(const std::string_view& id, GPUTextureHandle& outTexture);
    void Cache(const std::string_view& id, const GPUTextureHandle);

   private:
    std::unordered_map<std::string, GPUTextureHandle> m_texturePool;
};
}  // namespace OneGame::Engine
