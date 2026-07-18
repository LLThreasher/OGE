#pragma once

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

class SubsystemPipeline : public FixedStepPipeline<Subsystem>
{
public:
    SubsystemPipeline(GameState& state, AnythingFactory& af) : FixedStepPipeline<Subsystem>(state, af) {}
};
}  // namespace oge::runtime
