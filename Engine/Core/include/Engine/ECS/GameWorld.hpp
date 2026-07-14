#pragma once

#include <vector>

#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/ECS/NetClient.hpp"
#include "Engine/ECS/NetServer.hpp"
#include "Engine/ECS/SubsystemRegistry.hpp"
#include "Engine/Terrain/TerrainService.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine::ECS
{

class GameRenderer;

class GameWorld
{
   public:
    void CreateTerrain()
    {
        m_world.ctx().emplace<Terrain::BlockRegistry>();
        m_world.ctx().emplace<Terrain::TerrainView>();
        m_world.ctx().emplace<Terrain::TerrainDesc>();
    }

    void CreateServer() { m_world.ctx().emplace<NetServer>(); }

    void CreateClient() { m_world.ctx().emplace<NetClient>(); }

    GameWorldContext& Get() { return m_world; }

   private:
    entt::registry m_world;
};

struct SubsystemCollection
{
    std::vector<std::unique_ptr<SubsystemBase>> subsystems;
    struct Def
    {
        std::vector<std::string> subsystems;
    };
    static SubsystemCollection Build(Def def, AppContext& ctx)
    {
        return {DefBuilder::BuildABCVec<SubsystemBase>(def.subsystems, ctx.subsystemRegistry)};
    }
};

struct TickGroup : SubsystemCollection
{
    bool isFrame;
    TickScheduler tick;

    struct Def : SubsystemCollection::Def
    {
        bool isFrame;
        float tickInterval;
    };

    static TickGroup Build(Def def, AppContext& ctx)
    {
        return {
            SubsystemCollection::Build(static_cast<SubsystemCollection::Def>(def), ctx),
            def.isFrame,
            TickScheduler(def.tickInterval),
        };
    }
};

class GameUpdateScheduler
{
   public:
    struct Def
    {
        std::vector<TickGroup::Def> tickGroups;
    };
    class Builder
    {
       public:
        Builder&& WithTickGroup(std::string_view ty, float interval, bool isFrame = false) &&
        {
            m_tickGrpLookup.emplace(ty, m_def.tickGroups.size());
            m_def.tickGroups.emplace_back(TickGroup::Def{{}, isFrame, interval});
            return std::move(*this);
        }

        template <typename TSys>
        Builder&& With(std::string_view ty = "Frame") &&
        {
            m_def.tickGroups.at(m_tickGrpLookup.at(std::string(ty))).subsystems.emplace_back(TSys::Name);
            return std::move(*this);
        }

        GameUpdateScheduler Build(AppContext& ctx) && { return GameUpdateScheduler::Build(m_def, ctx); }

       private:
        std::unordered_map<std::string_view, size_t> m_tickGrpLookup = {{"Frame", 0u}};
        Def m_def = {{TickGroup::Def{{}, true, 1 / 60.f}}};
    };

    GameUpdateScheduler(std::vector<TickGroup> subsystems = {}) : m_subsystems(std::move(subsystems)) {}

    void Initialize(GameWorldContext& game, AppContext ctx)
    {
        for (auto& col : m_subsystems)
        {
            for (auto& ptr : col.subsystems)
            {
                ptr->Initialize(game, ctx);
            }
        }
    }

    void Update(GameWorldContext& game, AppContext app, const FrameInputData& frame)
    {
        for (auto& col : m_subsystems)
        {
            if (col.isFrame)
            {
                for (auto& ptr : col.subsystems)
                {
                    ptr->Update(game, app, frame);
                }
            }
            else
            {
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
    }

    static GameUpdateScheduler Build(Def definition, AppContext& ctx)
    {
        return GameUpdateScheduler(DefBuilder::BuildVec<TickGroup>(definition.tickGroups, ctx));
    }

   private:
    std::vector<TickGroup> m_subsystems;
};
}  // namespace OneGame::Engine::ECS
