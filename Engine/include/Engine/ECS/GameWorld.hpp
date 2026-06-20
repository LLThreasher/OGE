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

		void Initialize(AppContext& ctx)
		{
			SubsystemInitContext sctx
			{
				ctx
			};
			for (auto& ptr : m_subsystems)
			{
				ptr->Initialize(sctx);
			}
		}

		void Update(AppContext& app, FrameContext& frame)
		{
			SubsystemContext sctx
			{
				frame.dt,
				frame.input,
				app.events,
				app.backend,
			};
			for (auto& ptr : m_subsystems)
			{
				ptr->Update(sctx);
				ptr->Present(frame.presentationWorld);
			}
		}

	private:
		std::vector<std::unique_ptr<ISubsystem>> m_subsystems;
		entt::registry m_world;
	};
}
