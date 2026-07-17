#pragma once

#include "oge/runtime/entt.hpp"
#include "oge/runtime/staged_scheduler.hpp"

namespace oge::runtime
{

struct GameState
{
    entt::registry& world;
    entt::dispatcher& events;
};

struct FGameState : GameState
{
    float dt;
};

class Subsystem : public Stage<GameState, FGameState>
{
   public:
    Subsystem(oge_id_type id) : Stage<GameState, FGameState>(id) {}
};
}  // namespace oge::runtime
