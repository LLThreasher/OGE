#pragma once

#include <string>
#include "Engine/GameAppState.hpp"

namespace OneGame::Engine
{
	class IScene
	{
	public:
		virtual ~IScene() = default;

		virtual void Initialize(AppInitContext& context) = 0;
		virtual void Enter(AppContext& context) = 0;
		virtual void Exit(AppContext& context) = 0;
		virtual void Update(AppContext& context, float dt) = 0;
	};
}
