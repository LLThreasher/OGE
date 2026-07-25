#pragma once

#include "game/app_context.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::OGEContext;

class Scene : protected AppRuntime
{
   protected:
    entt::registry m_world;
    sim::SubsystemPipeline m_subsystems;
    sim::RealtimeSubsystemPipeline m_realtimeSubsystems;

   public:
    struct Frame
    {
        float dt;
    };

    struct Def
    {
        AppContext ctx;
        const json::Value& args;
        OGEContext& rctx;
    };

    DECL_ID(Scene)
    Scene(const Def& def)
        : AppRuntime(def.ctx),
          m_subsystems({m_world, def.ctx.events, def.ctx.memory}, 1.f / 30.f),
          m_realtimeSubsystems({m_world, def.ctx.events, def.ctx.memory})
    {
    }

    virtual ~Scene() {}

    virtual void Update(Frame f)
    {
        m_subsystems.Update(f.dt);
        m_realtimeSubsystems.Update(f.dt);
    }

    virtual void Load(const SceneConfig& config)
    {
        for (auto stage : config.subsystems)
        {
            m_subsystems.AddStage(m_ctx.any_factory, stage);
        }
        for (auto stage : config.realtimeSubsystems)
        {
            m_realtimeSubsystems.AddStage(m_ctx.any_factory, stage);
        }
    }

    virtual void Unload()
    {
        m_subsystems.Clear();
        m_realtimeSubsystems.Clear();
    }
};

}  // namespace game
