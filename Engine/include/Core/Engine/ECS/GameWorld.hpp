#pragma once

#include <vector>

#include "Engine/entt.hpp"
#include "ISubsystem.hpp"
#include "Engine/Terrain/TerrainService.hpp"

namespace OneGame::Engine::ECS
{
class GameRenderer;
class GameWorld
{
    friend class GameRenderer;
   public:
    GameWorld()
    {
        GameWorldContext& game = Get();
        game.ctx().emplace<Terrain::BlockRegistry>();
        game.ctx().emplace<Terrain::TerrainView>();
    }

    template <typename TSubsystem>
    void Register()
    {
        m_subsystems.emplace_back(new TSubsystem());
    }

    void Initialize(AppContext ctx)
    {
        GameWorldContext& game = Get();
        for (auto& ptr : m_subsystems)
        {
            ptr->Initialize(game, ctx);
        }
    }

    void Update(AppContext app, const FrameInputData& frame)
    {
        GameWorldContext& game = Get();
        for (auto& ptr : m_subsystems)
        {
            ptr->Update(game, app, frame);
        }
    }

    GameWorldContext& Get() { return m_world; }

   private:
    std::vector<std::unique_ptr<SubsystemBase>> m_subsystems;
    entt::registry m_world;
};
}  // namespace OneGame::Engine::ECS
