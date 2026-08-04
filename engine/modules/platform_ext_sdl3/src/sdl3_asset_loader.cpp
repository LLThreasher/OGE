#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_storage.h"
#if defined(IO_USE_SDL3)
#include <SDL3/SDL.h>

#include "oge/log.hpp"
#include "oge/platform/io.hpp"

namespace oge::platform
{
bool TryLoadBlob(const std::string_view& id, std::vector<char>& output)
{
    size_t fileSize = 0;

#ifdef PLATFORM_ANDROID
    void* fileBuffer = SDL_LoadFile(std::string(id).c_str(), &fileSize);
#else
    std::string path = fmt::format("{}assets/{}", SDL_GetBasePath(), id);
    void* fileBuffer = SDL_LoadFile(path.c_str(), &fileSize);
#endif

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

bool TrySaveBlob(const std::string_view& id, const std::vector<char>& output)
{
#ifdef PLATFORM_ANDROID
    return SDL_SaveFile(std::string(id).c_str(), output.data(), output.size());
#else
    std::string path = fmt::format("{}assets/{}", SDL_GetBasePath(), id);
    return SDL_SaveFile(path.c_str(), output.data(), output.size());
#endif
}
}  // namespace oge::platform
#endif
