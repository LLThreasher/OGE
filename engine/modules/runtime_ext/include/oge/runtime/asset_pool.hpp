#pragma once

#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "oge/graphics/objects.hpp"

namespace oge::graphics
{
class IGraphicsBackend;
}

namespace oge::runtime
{
class AssetManager;
class StreamingManager;
namespace ui
{
class IFont;
}

class AssetPool
{
   public:
    ~AssetPool() = default;

    bool Load(const std::string_view& id, GPUTextureHandle& outTexture);
    void Cache(const std::string_view& id, const GPUTextureHandle);

    bool Load(const std::string_view& id, std::shared_ptr<ui::IFont>& font);
    void Cache(const std::string_view& id, const std::shared_ptr<ui::IFont>);

   private:
    std::unordered_map<std::string, GPUTextureHandle> m_texturePool;
    std::unordered_map<std::string, std::shared_ptr<ui::IFont>> m_fontPool;
};
}  // namespace OneGame::Engine
