#pragma once

#include <vector>

#include "Engine/ECS/NetClient.hpp"
#include "Engine/ECS/NetServer.hpp"
#include "Engine/Terrain/TerrainService.hpp"
#include "Engine/entt.hpp"
#include "Engine/TickScheduler.hpp"
#include "ISubsystem.hpp"

namespace OneGame::Engine::ECS
{

enum class TickType : uint32_t
{
    Frame = 0,
    Physics,
    Network,
};

const std::array<TickType, 3> ALL_TICK_TYPES = {
    TickType::Frame,
    TickType::Physics,
    TickType::Network,
};

const std::unordered_map<TickType, float> ALL_TICK_FIX_DELTA = {
    {TickType::Frame, 1 / 60.f},
    {TickType::Physics, 1 / 60.f},
    {TickType::Network, 1 / 60.f},
};

class GameRenderer;
class GameWorld
{
    friend class GameRenderer;

    struct SubsystemCollection
    {
        TickScheduler tick;
        std::vector<std::unique_ptr<SubsystemBase>> subsystems;
    };

   public:
    void CreateTerrain()
    {
        m_world.ctx().emplace<Terrain::BlockRegistry>();
        m_world.ctx().emplace<Terrain::TerrainView>();
        m_world.ctx().emplace<Terrain::TerrainDesc>();
    }

    void CreateServer() { m_world.ctx().emplace<NetServer>(); }

    void CreateClient() { m_world.ctx().emplace<NetClient>(); }

    template <typename TSubsystem, TickType tickTy = TickType::Frame>
    void Register()
    {
        auto it = m_subsystems.find(tickTy);
        if (it == m_subsystems.end())
        {
            SubsystemCollection v{TickScheduler(ALL_TICK_FIX_DELTA.at(tickTy))};
            auto [newIt, _] = m_subsystems.emplace(tickTy, std::move(v));
            it = newIt;
        }
        m_subsystems[tickTy].subsystems.emplace_back(new TSubsystem());
    }

    void Initialize(AppContext ctx)
    {
        GameWorldContext& game = Get();
        for (auto tickTy : ALL_TICK_TYPES)
        {
            auto& col = m_subsystems[tickTy];
            for (auto& ptr : col.subsystems)
            {
                ptr->Initialize(game, ctx);
            }
        }
    }

    void Update(AppContext app, const FrameInputData& frame)
    {
        GameWorldContext& game = Get();
        {
            auto& col = m_subsystems[TickType::Frame];
            for (auto& ptr : col.subsystems)
            {
                ptr->Update(game, app, frame);
            }
        }
        for (uint32_t i = 1; i < ALL_TICK_TYPES.size(); ++i)
        {
            auto& col = m_subsystems[ALL_TICK_TYPES[i]];
            col.tick.Poll(frame.dt);
            auto frameClone = frame;
            while ((frameClone.dt = col.tick.ConsumeTick()) > 0.f)
            {
                for (auto& ptr : col.subsystems)
                {
                    ptr->Update(game, app, frameClone);
                }
            }
        }
    }

    GameWorldContext& Get() { return m_world; }

   private:
    std::unordered_map<TickType, SubsystemCollection> m_subsystems;
    entt::registry m_world;
};
}  // namespace OneGame::Engine::ECS
