#pragma once

#include <cinttypes>
#include <vector>

namespace oge
{
struct TextureInfo
{
    uint32_t width;
    uint32_t height;
};

struct TextureData
{
    TextureInfo info;
    std::vector<char> data;
};
}  // namespace oge
