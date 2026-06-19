#pragma once

#include <vector>
#include <entt/entt.hpp>
#include "ISubsystem.hpp"

namespace OneGame::Engine::ECS
{
	class GameWorld
	{
	public:
		template <typename TSubsystem>
		void Register()
		{
			m_subsystems.emplace_back(new TSubsystem(m_world));
		}

		void Initialize(AppInitContext& ctx)
		{
			SubsystemInitContext sctx
			{
				ctx.backend,
				ctx.events,
			};
			for (auto& ptr : m_subsystems)
			{
				ptr->Initialize(sctx);
			}
		}

		void Update(AppContext& ctx, float dt)
		{
			SubsystemContext sctx
			{
				ctx.backend,
				ctx.input,
				ctx.events,
			};
			for (auto& ptr : m_subsystems)
			{
				ptr->Update(sctx, dt);
				ptr->Present(ctx.world);
			}
		}
	private:
		std::vector<std::unique_ptr<ISubsystem>> m_subsystems;
		entt::registry m_world;
	};
}
