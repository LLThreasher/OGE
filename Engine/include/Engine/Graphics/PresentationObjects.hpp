#pragma once
#include "Engine/Math.hpp"

namespace OneGame::Engine::Graphics
{
	struct PViewTransform
	{
		math::mat4 view;
	};

	// always drawn on top left corner
	struct PDebugText
	{
		std::string text;
	};
}
