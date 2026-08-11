#pragma once

#include <uuid.h>

#include <cstddef>

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/json.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/runtime/type_name.hpp"

namespace game
{
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::OGEContext;

class Scene : protected AppRuntime
{
    enum class LifetimeStage
    {
        Unloaded = 0,
        Loaded,
    };

#if OGE_DEBUG
    // Must be initialized: Scene::Load asserts on it, and the constructor
    // does not touch it.  A garbage value would randomly trip "double load".
    LifetimeStage m_lifetimeStage = LifetimeStage::Unloaded;
#endif

   protected:
    GameWorld m_world;
    sim::SubsystemPipeline m_subsystems;
    sim::RealtimeSubsystemPipeline m_realtimeSubsystems;

    SceneConfig m_sceneConfig = {};

   public:
    SceneConfig& GetConfig() { return m_sceneConfig; }
    const SceneConfig& GetConfig() const { return m_sceneConfig; }

    auto& GetWorld() { return m_world; }
    const auto& GetWorld() const { return m_world; }

    // Interpolation alpha from the fixed-step scheduler.
    // Range [0, 1) — fraction of the way from the previous physics tick
    // toward the next.  Renderers use this to smooth entity positions.
    float GetFixedStepAlpha() const { return m_subsystems.GetAlpha(); }

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
