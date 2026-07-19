#pragma once

#include <string>
#include <vector>

namespace oge::runtime
{
class AssetManager;
class OGEContextReadOnly;

struct AssetBase
{
    AssetManager& assetManager;

    AssetBase(OGEContextReadOnly& ctx);
    bool LoadBlob(const std::string_view& id, std::vector<char>& data);
};
} // namespace oge::runtime
