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
			m_subsystems.emplace_back(new TSubsystem());
		}

		void Initialize(AppContext& ctx)
		{
			for (auto& ptr : m_subsystems)
			{
				ptr->Initialize(ctx, m_world);
			}
		}

		void Update(AppContext& app, const FrameInputData& frame)
		{
			for (auto& ptr : m_subsystems)
			{
				ptr->Update(app, m_world, frame);
			}
		}

		void Present(PresentationContext& pctx, FrameOutputData& frameOut)
		{
			for (auto& ptr : m_subsystems)
			{
				ptr->Present(m_world, pctx, frameOut);
			}
		}

	private:
		std::vector<std::unique_ptr<ISubsystem>> m_subsystems;
		entt::registry m_world;
	};
}
