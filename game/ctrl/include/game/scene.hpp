#pragma once

#include <optional>
#include <variant>

#include "game/app_context.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene_runner.hpp"
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

    SceneConfig m_sceneConfig = {};

   public:
    struct Frame
    {
        float dt;
    };

    struct Def
    {
        AppContext ctx;
        const json::Object& args;
    };

    DECL_ID(Scene)
    Scene(const Def& def);

    virtual ~Scene();
    virtual void Update(Frame f, SceneContext sctx);
    virtual void Load();
    virtual void Unload();
};

}  // namespace game
