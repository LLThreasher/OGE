#ifdef USE_SDL3
#ifdef PLATFORM_ANDROID

#include <SDL3/SDL.h>
#include <format>
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

        // SDL3 allocates the buffer and reads the file from APK assets
        void* fileBuffer = SDL_LoadFile(std::string(id).c_str(), &fileSize);

        if (fileBuffer == NULL) {
            LOG_ERROR("Failed to load file into memory: %s", SDL_GetError());
            return false;
        }

        output.resize(fileSize);
        memcpy(output.data(), fileBuffer, fileSize);

        // You MUST free the buffer using SDL_free when done
        SDL_free(fileBuffer);

        return true;
	}
}
#endif
#endif
