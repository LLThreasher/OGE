#pragma once

#include <vector>

#include "Engine/entt.hpp"
#include "ISubsystem.hpp"
#include "Engine/Terrain/Terrain.hpp"

namespace OneGame::Engine::ECS
{
class GameWorld
{
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
        m_terrain.Initialize(game, ctx);
        for (auto& ptr : m_subsystems)
        {
            ptr->Initialize(game, ctx);
        }
    }

    void InitializeWithPresent(PresentationContext ctx);

    void Update(AppContext app, const FrameInputData& frame)
    {
        GameWorldContext& game = Get();
        m_terrain.Update(game, app, frame);
        for (auto& ptr : m_subsystems)
        {
            ptr->Update(game, app, frame);
        }
    }

    void Present(PresentationContext pctx, FrameOutputData& frameOut)
    {
        GameWorldContext& game = Get();
        m_terrain.Present(game, pctx, frameOut);
        for (auto& ptr : m_subsystems)
        {
            ptr->Present(game, pctx, frameOut);
        }
    }

    GameWorldContext& Get() { return m_world; }

   private:
    std::vector<std::unique_ptr<SubsystemBase>> m_subsystems;
    entt::registry m_world;
    Terrain::TerrainService m_terrain;
};
}  // namespace OneGame::Engine::ECS
