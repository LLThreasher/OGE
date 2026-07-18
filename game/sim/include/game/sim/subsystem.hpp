#pragma once

#include "game/frame_perf.hpp"  // debug info pass
#include "oge/runtime/entt.hpp"
#include "oge/runtime/staged_scheduler.hpp"

namespace game::sim
{
using namespace oge::runtime;

struct GameState
{
    entt::registry& world;
    entt::dispatcher& events;
};

struct FGameState : GameState
{
    float dt;
    FGameState(float dt, GameState& state) : dt(dt), GameState(state) {}
};

class Subsystem : public Stage<GameState, FGameState>
{
   public:
    Subsystem(oge_id_type id) : Stage<GameState, FGameState>(id) {}
};

class SubsystemPipeline : public FixedStepPipeline<Subsystem, float>
{
   public:
    SubsystemPipeline(GameState& state, AnythingFactory& af) : FixedStepPipeline<Subsystem, float>(state, af) {}
};

class SubsystemDebugText : public Subsystem
{
    float currentFPS = 0.f;
    float currentFrameTime = 0.f;
    float accumTime = 0.f;
    uint64_t frameCount = 0;
    FramePerfStatus totalPerfStatus = {};
    FramePerfStatus perfStatus = {};

   public:
    static constexpr oge_id_type Id = entt::hashed_string("SubsystemDebugText").value();
    SubsystemDebugText() : Subsystem(Id) {}
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};
}  // namespace game::sim
