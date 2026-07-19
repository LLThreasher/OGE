#pragma once

#include <string>
#include <unordered_map>

#include "oge/macros.hpp"
#include "oge/math.hpp"
#include "oge/texture_data.hpp"
#include "oge/mesh_data.hpp"

namespace oge::runtime
{
class AssetManager
{
   public:
    AssetManager() {}
    NO_COPY(AssetManager)
    TextureInfo* GetTextureInfo(const std::string_view& id);
    TextureData* LoadTexture(const std::string_view& id);
    MeshData* LoadMesh(const std::string_view& id);

   private:
    std::unordered_map<std::string, TextureData> m_textures;
    std::unordered_map<std::string, MeshData> m_meshes;
};
}  // namespace OneGame::Engine
