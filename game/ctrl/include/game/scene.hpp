#pragma once

#include "game/app_context.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/runtime/asset_base.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "game/json.hpp"

namespace game
{
using oge::runtime::AnythingFactory;
using oge::runtime::OGEContext;

class Scene
{
   public:
    struct Ctx
    {
        OGEContext& ctx;
        oge::runtime::AssetBase assets;

        Ctx(OGEContext& ctx) : ctx(ctx), assets(ctx) {}
    };

    struct Frame
    {
        float dt;
    };

   protected:
    MemoryContext m_memory = {{8*1024}, {1*256*1024, 10.f}}; // 8k per frame, 256k per 5 sec
    std::optional<Ctx> m_ctx;

    sim::GameState m_gameState;
    entt::registry m_world;
    sim::SubsystemPipeline m_subsystems;

   public:
    Scene(AppContext ctx)
        : m_gameState(m_world, ctx.events, m_memory),
          m_subsystems(m_gameState, ctx.any_factory, 1.f / 30.f)
    {
    }

    virtual void Attach(const json::Value& args, OGEContext& ctx, AnythingFactory& af)
    {
        m_ctx.emplace(ctx);
    }

    virtual void Update(Frame f) { m_subsystems.Update(f.dt); }

    virtual void Detach() { m_ctx.reset(); }
};
}  // namespace game
