#pragma once

#include <vector>
#include <span>
#include <string>
#include <unordered_map>
#include <memory>

#include "Engine/ObjectType.hpp"

namespace OneGame::Engine
{
class AssetManager;
class StreamingManager;
namespace Graphics
{
class IGraphicsBackend;
}
namespace UI
{
class IFont;
}

class AssetPool
{
   public:
    ~AssetPool() = default;

    bool Load(const std::string_view& id, GPUTextureHandle& outTexture);
    void Cache(const std::string_view& id, const GPUTextureHandle);

    bool Load(const std::string_view& id, std::shared_ptr<UI::IFont>& font);
    void Cache(const std::string_view& id, const std::shared_ptr<UI::IFont>);

   private:
    std::unordered_map<std::string, GPUTextureHandle> m_texturePool;
    std::unordered_map<std::string, std::shared_ptr<UI::IFont>> m_fontPool;
};
}  // namespace OneGame::Engine
