#pragma once
#include <string>
#include <entt/entt.hpp>

namespace OneGame::Engine
{
	struct AppContext;

	class IScene
	{
	public:
		virtual ~IScene() = default;

		virtual void Initialize(AppContext& context) = 0;
		virtual void Enter(AppContext& context) = 0;
		virtual void Exit(AppContext& context) = 0;
		virtual void FixedUpdate(AppContext& context, float dt) = 0;
	};
}
