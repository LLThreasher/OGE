#pragma once

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
   public:
    Subsystem(oge_id_type id) : Stage<GameState, FGameState>(id) {}
};

class RealtimeSystem : public Subsystem
{
   public:
    struct Def
    {
        union
        {
            SubsystemPhysicsData subsystemPhysicsData;
        };
    };
    RealtimeSystem(oge_id_type id) : Subsystem(id) {}
};

class SubsystemPipeline : public FixedStepPipeline<Subsystem, float>
{
   public:
    SubsystemPipeline(GameState& state, AnythingFactory& af, float updateInterval)
        : FixedStepPipeline<Subsystem, float>(state, af, updateInterval)
    {
    }
};

class RealtimeSubsystemPipeline : public FramePipeline<RealtimeSystem, float>
{
   public:
    RealtimeSubsystemPipeline(GameState& state, AnythingFactory& af) : FramePipeline<RealtimeSystem, float>(state, af)
    {
    }
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
        SysName() : Subsystem(Id) {} \
    };

#define DECL_REALTIME_SYS(SysName)                                                        \
    class SysName : public RealtimeSystem                                                 \
    {                                                                                     \
        DECL_FNS(SysName)                                                                 \
        SysName() : RealtimeSystem(Id) {}                                                 \
        static std::unique_ptr<RealtimeSystem> Build(const Def& def, AnythingFactory& af) \
        {                                                                                 \
            return std::make_unique<SysName>();                                           \
        }                                                                                 \
    };

class SubsystemDebugText : public Subsystem
{
    float currentFrameTime = 0.f;
    float accumTime = 0.f;
    uint64_t frameCount = 0;
    FramePerfStatus totalPerfStatus = {};
    FramePerfStatus perfStatus = {};
    DECL_FNS(SubsystemDebugText)
    SubsystemDebugText() : Subsystem(Id) {}
};

class SubsystemCreature : public RealtimeSystem
{
    SubsystemPhysicsData m_data;

   public:
    DECL_ID(SubsystemCreature)
    SubsystemCreature() : m_data({}), RealtimeSystem(Id) {}
    SubsystemCreature(SubsystemPhysicsData data) : m_data(data), RealtimeSystem(Id) {}
    static std::unique_ptr<RealtimeSystem> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<SubsystemCreature>(def.subsystemPhysicsData);
    }
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};

class SubsystemPlayer : public RealtimeSystem
{
    SubsystemPhysicsData m_data;

   public:
    DECL_ID(SubsystemPlayer)
    SubsystemPlayer() : m_data({}), RealtimeSystem(Id) {}
    SubsystemPlayer(SubsystemPhysicsData data) : m_data(data), RealtimeSystem(Id) {}
    static std::unique_ptr<RealtimeSystem> Build(const Def& def, AnythingFactory& af)
    {
        return std::make_unique<SubsystemPlayer>(def.subsystemPhysicsData);
    }
    void onAttach(GameState& ctx) override;
    void onDetach(GameState& ctx) override;
    void onUpdate(FGameState& ctx) override;
};

}  // namespace game::sim

#undef DECL_FNS
