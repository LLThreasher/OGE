#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_DARWIN) || defined(PLATFORM_LINUX)

#include <format>
#include <fstream>
#include <vector>

#define LOGGER_NAME "Engine"
#include "oge/log.hpp"
#include "oge/platform/io.hpp"
#include "stb_image.h"

namespace oge::platform
{
bool TryLoadBlob(const std::string_view& id, std::vector<char>& output)
{
    auto filePath = std::format("assets/{}", id);
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        LOG_ERROR("Failed to open file {}", filePath);
        return false;
    }

    // 2. Get the file size and allocate the vector buffer
    std::streamsize size = file.tellg();
    output.resize(size);

    // 3. Move the file pointer back to the beginning and read the data
    file.seekg(0, std::ios::beg);
    file.read(output.data(), size);
    return true;
}
}  // namespace oge::platform
#endif

namespace oge::platform
{
static bool TryLoadPNG(std::vector<char> data, int& width, int& height, void* result)
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
}
