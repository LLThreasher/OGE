#include "oge/runtime/asset_manager.hpp"

#include <cstring>

#include "oge/platform/io.hpp"
#include "oge/runtime/asset_base.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace oge::runtime
{
using namespace platform;

TextureInfo* AssetManager::GetTextureInfo(const std::string_view& id)
{
    auto str_id = std::string(id);
    auto it = m_textures.find(str_id);
    if (it != m_textures.end())
    {
        return &it->second.info;
    }
    else
    {
        return &LoadTexture(id)->info;
    }
}

TextureData* AssetManager::LoadTexture(const std::string_view& id)
{
    auto str_id = std::string(id);
    auto it = m_textures.find(str_id);
    if (it != m_textures.end())
    {
        return &it->second;
    }

    TextureData data;
    std::vector<char> blob;
    if (!TryLoadBlob(id, blob)) return nullptr;
    if (!TryLoadPNG(blob, data)) return nullptr;
    m_textures.emplace(id, data);
    return &m_textures[str_id];
}

AssetBase::AssetBase(OGEContext& ctx) : assetManager(*ctx.Get<AssetManager>()) {}

bool AssetBase::LoadBlob(const std::string_view& id, std::vector<char>& data) { return TryLoadBlob(id, data); }

}  // namespace oge::runtime
