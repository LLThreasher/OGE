#ifdef USE_SDL3
#include <SDL3/SDL.h>

#ifndef PLATFORM_ANDROID
#include <format>
#endif

#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
using namespace Graphics;

bool TryLoadBlob(const std::string_view& id, std::vector<char>& output)
{
    size_t fileSize = 0;

#ifdef PLATFORM_ANDROID
    void* fileBuffer = SDL_LoadFile(std::string(id).c_str(), &fileSize);
#else
    const char* basePath = SDL_GetBasePath();
    auto filePath = std::format("{}/assets/{}", basePath, id);
    void* fileBuffer = SDL_LoadFile(filePath.c_str(), &fileSize);
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
}  // namespace OneGame::Engine
#endif
