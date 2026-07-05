#ifndef USE_SDL3
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_DARWIN)

#include <format>
#include <fstream>

#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
using namespace Graphics;

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
}  // namespace OneGame::Engine
#endif
#endif
