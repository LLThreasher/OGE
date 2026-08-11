#include "oge/platform/io.hpp"

#include <cstring>
#include <vector>

#include "stb_image.h"

namespace oge::platform
{
static bool TryLoadPNG(std::vector<char> data, int& width, int& height,
                       void* result)
{
    int texChannels;
    if (result == nullptr)
    {
        if (!stbi_info_from_memory((unsigned char*)(data.data()), data.size(),
                                   &width, &height, &texChannels))
        {
            // LOG_ERROR("Failed to read image info! {}", filePath);
            return false;
        }
        return true;
    }

    stbi_uc* pixels =
        stbi_load_from_memory((unsigned char*)(data.data()), data.size(),
                              &width, &height, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        // LOG_ERROR("Failed to load texture! {}", filePath);
        return false;
    }

    std::memcpy(result, pixels, width * height * sizeof(char) * 4);

    stbi_image_free(pixels);
    return true;
}

bool TryLoadPNG(std::vector<char> blob, TextureData& data)
{
    int iwidth, iheight;
    if (!TryLoadPNG(blob, iwidth, iheight, nullptr)) return false;
    data.info.width = iwidth;
    data.info.height = iheight;
    data.data.resize(data.info.width * data.info.height * sizeof(char) * 4 * 2);
    if (!TryLoadPNG(blob, iwidth, iheight, data.data.data())) return false;
    return true;
}
}  // namespace oge::platform
