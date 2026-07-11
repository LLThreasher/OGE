#include <fstream>

#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Vulkan/Vulkan.hpp"
#include "stb_image.h"

#define LOGGER_NAME "Vulkan"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::Graphics
{
std::unique_ptr<IGraphicsBackend> CreateBackend(BackendType type)
{
    if (type == BackendType::Vulkan)
    {
        return std::make_unique<Vulkan::VulkanBackend>();
    }
    else
    {
        throw std::runtime_error("undefined backend");
    }
}

bool TryLoadShaderBinary(const char* filePath, std::vector<char>& output)
{
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

bool TryLoadPNG(const char* filePath, int& width, int& height, void** result)
{
    int texChannels;
    if (result == nullptr)
    {
        if (!stbi_info(filePath, &width, &height, &texChannels))
        {
            LOG_ERROR("Failed to read image info! {}", filePath);
            return false;
        }
        return true;
    }

    stbi_uc* pixels = stbi_load(filePath, &width, &height, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        LOG_ERROR("Failed to load texture! {}", filePath);
        return false;
    }

    std::memcpy(*result, pixels, width * height * sizeof(char) * 4);

    stbi_image_free(pixels);
    return true;
}
}  // namespace OneGame::Engine::Graphics
