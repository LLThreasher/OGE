#pragma once

#include <uuid.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/platform/io.hpp"
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
    SceneConfig& GetConfig() { return m_sceneConfig; }
    const SceneConfig& GetConfig() const { return m_sceneConfig; }

    auto& GetWorld() { return m_world; }
    const auto& GetWorld() const { return m_world; }

    struct Frame
    {
        float dt;
    };

    struct Def
    {
        AppContext ctx;
        const json::Object& args;
    };

    Scene(const Def& def);

    virtual ~Scene();
    virtual void Update(Frame f, SceneContext sctx);
    virtual void Load();
    virtual void Unload();
};

SceneConfig GetDefaultSceneConfig(AnythingFactory&);
PlayerInfo LoadOrCreatePlayer();

}  // namespace game

DECL_TYPE_NAME(game::Scene, "core::Scene")
