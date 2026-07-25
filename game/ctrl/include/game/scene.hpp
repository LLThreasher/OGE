#pragma once

#include <memory>
#include "game/app_context.hpp"
#include "game/memory_context.hpp"
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
    struct Frame
    {
        float dt;
    };

    class Instance
    {
    protected:
        Scene& m_scene;
    public:
        Instance(Scene& scene, const json::Value& args, OGEContext& ctx, AnythingFactory& af) : m_scene(scene)
        {
        }

        virtual ~Instance()
        {
        }
        
        virtual void Update(Frame f)
        {
            m_scene.m_subsystems.Update(f.dt);
        }
    };

    entt::registry m_world;
    sim::SubsystemPipeline m_subsystems;
    AnythingFactory& m_af;

   public:
    Scene(AppContext ctx)
        : m_af(ctx.any_factory), m_subsystems({m_world, ctx.events, ctx.memory}, 1.f / 30.f)
    {
    }

    std::unique_ptr<Instance> Attach(const json::Value& args, OGEContext& ctx)
    {
        return std::make_unique<Instance>(*this, args, ctx, m_af);
    }
};
}  // namespace game
