#pragma once

#include <format>
#include <iterator>
#include <memory_resource>
#include <string>

#include "game/components.hpp"
#include "game/memory_context.hpp"
#include "game/frame_perf.hpp"  // debug info pass
#include "game/input/player_input_stream.hpp"
#include "oge/platform/perf.hpp"
#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/staged_scheduler.hpp"

namespace game::sim
{
using namespace oge::runtime;

struct GameState
{
    entt::registry& world;
    entt::dispatcher& events;
    MemoryContext& memory;
};

using GameFrame = float;

struct FGameState : GameState
{
    float dt;
    FGameState(GameFrame f, GameState& state) : dt(f), GameState(state) {}
};

struct ShowDebugTextEvent
{
    std::pmr::string text;
};

class Subsystem : public Stage<GameState, FGameState>
{
   protected:
    template <typename FMT, typename... Args>
    void ShowDebugText(FGameState& f, FMT& fmt, Args&... args)
    {
        std::pmr::string msg{f.memory.frameBuffer.Resource()};
        fmt::format_to(std::back_insert_iterator(msg), fmt, args...);
        f.events.trigger<ShowDebugTextEvent>({msg});
    }
};

class SubsystemPipeline : public FixedStepPipeline<Subsystem, GameFrame>
{
   public:
    NO_COPY(SubsystemPipeline)
    SubsystemPipeline(GameState& state, AnythingFactory& af,
                      float updateInterval)
        : FixedStepPipeline<Subsystem, GameFrame>(state, af, updateInterval)
    {
    }
};

class RealtimeSubsystemPipeline : public FramePipeline<Subsystem, GameFrame>
{
   public:
    RealtimeSubsystemPipeline(GameState& state, AnythingFactory& af)
        : FramePipeline<Subsystem, GameFrame>(state, af)
    {
    }
};

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
    oge::platform::RAMInfo ramInfo = {};
    double cpuUsage = {};
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
