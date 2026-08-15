#pragma once

#include <uuid.h>

#include <cstddef>
#include <cstdint>

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/game_world.hpp"
#include "oge/json.hpp"
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

    // Scene-driven fixed-frame accumulator (D2): render dt accumulates until
    // the fixed-frame duration is reached, then the fixed pipeline executes
    // as explicit sub-steps of sim::kSubStepDt (the sub-step boundaries are
    // visible to the stages via SimTickContext.subStepIdx).
    float m_fixedAccum = 0.f;
    float m_fixedFrameDuration = 1.f / 30.f;

    // Tick arbitration: the last SimTickContext.currentTick this scene saw.
    // Transport scenes (server/client) write the tick before Update — their
    // tick space is the replication tick.  A bare scene owns its tick
    // space: when the tick is unchanged, Update advances it and aggregates
    // the local input (sim::AggregateLocalInputs) before the fixed stages.
    uint32_t m_lastFixedTick = 0;

    SceneConfig m_sceneConfig = {};

   public:
    SceneConfig& GetConfig() { return m_sceneConfig; }
    const SceneConfig& GetConfig() const { return m_sceneConfig; }

    auto& GetWorld() { return m_world; }
    const auto& GetWorld() const { return m_world; }

    // Override the fixed-frame duration.  Default 1/30 preserves the
    // current fixed-pipeline behavior for all other scenes; the server
    // sets 1/20 (sim::kFixedFrameDuration).
    void SetFixedFrameDuration(float duration)
    {
        m_fixedFrameDuration = duration;
    }

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

    virtual GameWorld* GetAuthoritativeWorld()
    {
        return nullptr;
    }
};

SceneConfig GetDefaultSceneConfig(AnythingFactory&);
PlayerInfo LoadOrCreatePlayer();

}  // namespace game

DECL_TYPE_NAME(game::Scene, "core::Scene")
