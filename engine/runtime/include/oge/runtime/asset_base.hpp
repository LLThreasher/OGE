#pragma once

#include <string>
#include <vector>

namespace oge::runtime
{
class AssetManager;
class OGEContext;

struct AssetBase
{
    AssetManager& assetManager;

    AssetBase(OGEContext& ctx);
    bool LoadBlob(const std::string_view& id, std::vector<char>& data);
};
} // namespace oge::runtime
