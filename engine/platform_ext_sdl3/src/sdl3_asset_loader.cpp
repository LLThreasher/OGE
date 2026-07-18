#ifdef PLATFORM_ANDROID
#include <SDL3/SDL.h>

#include "oge/platform/io.hpp"
#include "oge/log.hpp"

namespace oge
{
bool TryLoadBlob(const std::string_view& id, std::vector<char>& output)
{
    size_t fileSize = 0;

    void* fileBuffer = SDL_LoadFile(std::string(id).c_str(), &fileSize);

    if (fileBuffer == NULL)
    {
        LOG_ERROR("Failed to load file into memory: {}", SDL_GetError());
        return false;
    }

    output.resize(fileSize);
    memcpy(output.data(), fileBuffer, fileSize);

    // You MUST free the buffer using SDL_free when done
    SDL_free(fileBuffer);

    return true;
}
}  // namespace OneGame::Engine
#endif
