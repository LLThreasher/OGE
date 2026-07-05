#include "Engine/AssetManager.hpp"

#include "stb_image.h"

namespace OneGame::Engine
{
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
    int iwidth, iheight;
    if (!TryLoadPNG(blob, iwidth, iheight, nullptr)) return nullptr;
    data.info.width = iwidth;
    data.info.height = iheight;
    data.data.resize(data.info.width * data.info.height * sizeof(char) * 4 * 2);
    if (!TryLoadPNG(blob, iwidth, iheight, data.data.data())) return nullptr;
    m_textures.emplace(id, data);
    return &m_textures[str_id];
}

bool TryLoadPNG(std::vector<char> data, int& width, int& height, void* result)
{
    int texChannels;
    if (result == nullptr)
    {
        if (!stbi_info_from_memory((unsigned char*)(data.data()), data.size(), &width, &height, &texChannels))
        {
            // LOG_ERROR("Failed to read image info! {}", filePath);
            return false;
        }
        return true;
    }

    stbi_uc* pixels = stbi_load_from_memory((unsigned char*)(data.data()), data.size(), &width, &height, &texChannels,
                                            STBI_rgb_alpha);

    if (!pixels)
    {
        // LOG_ERROR("Failed to load texture! {}", filePath);
        return false;
    }

    std::memcpy(result, pixels, width * height * sizeof(char) * 4);

    stbi_image_free(pixels);
    return true;
}
}  // namespace OneGame::Engine
