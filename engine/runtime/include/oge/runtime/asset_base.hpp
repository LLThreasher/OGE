#pragma once

#include <string>
#include <vector>

namespace oge::runtime
{
class AssetManager;

struct AssetBase
{
    AssetManager& assetManager;

    bool LoadBlob(const std::string_view& id, std::vector<char>& data);
};
} // namespace oge::runtime
