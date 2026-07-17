#pragma once

#include <string>
#include <vector>

#include "oge/texture_data.hpp"

namespace oge::platform
{
bool TryLoadPNG(std::vector<char> data, TextureData& outData);
bool TryLoadBlob(const std::string_view& id, std::vector<char>&);
}  // namespace oge::platform
