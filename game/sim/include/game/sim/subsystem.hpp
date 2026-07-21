#pragma once

#include "game/components.hpp"
#include "game/frame_perf.hpp"  // debug info pass
#include "game/input/player_input_stream.hpp"
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

struct SubsystemPhysicsData
{
    bool isFrame = false;
};

class Subsystem : public Stage<GameState, FGameState>
{
};

class SubsystemPipeline : public FixedStepPipeline<Subsystem, float>
{
   public:
    SubsystemPipeline(GameState& state, AnythingFactory& af, float updateInterval)
        : FixedStepPipeline<Subsystem, float>(state, af, updateInterval)
    {
    }
};

class RealtimeSubsystemPipeline : public FramePipeline<Subsystem, float>
{
   public:
    RealtimeSubsystemPipeline(GameState& state, AnythingFactory& af) : FramePipeline<Subsystem, float>(state, af) {}
};

void RegisterSubsystems(AnythingFactory& af);

#define DECL_FNS(SysName)                   \
   public:                                  \
    DECL_ID(SysName)                        \
    void onAttach(GameState& ctx) override; \
    void onDetach(GameState& ctx) override; \
    void onUpdate(FGameState& ctx) override;

#define DECL_SYS(SysName)            \
    class SysName : public Subsystem \
    {                                \
        DECL_FNS(SysName)            \
    };

class SubsystemDebugText : public Subsystem
{
    float currentFrameTime = 0.f;
    float accumTime = 0.f;
    uint64_t frameCount = 0;
    FramePerfStatus totalPerfStatus = {};
    FramePerfStatus perfStatus = {};
    DECL_FNS(SubsystemDebugText)
};

template <UpdateType utype>
class SubsystemCreature : public Subsystem
{
   public:
    DECL_ID(SubsystemCreature<utype>)
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};

template <UpdateType utype>
class SubsystemPlayer : public Subsystem
{
   public:
    DECL_ID(SubsystemPlayer<utype>)

    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};

#undef DECL_FNS
#undef DECL_SYS

#define DECL_UTYPES_IMPL(SysName)                  \
    template class SysName<UpdateType::FixedStep>; \
    template class SysName<UpdateType::Realtime>;

}  // namespace game::sim
