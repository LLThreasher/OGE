#ifdef USE_SDL3

#include <format>
#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

namespace OneGame::Engine
{
	using namespace Graphics;

	bool TryLoadBlob(const std::string_view& id, std::vector<char>&)
	{
	}
}
#endif
