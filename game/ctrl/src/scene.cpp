#include "game/scene.hpp"

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/input/player_input_stream.hpp"
#include "oge/json.hpp"
#include "game/net/replication_events.hpp"
#include "game/sim/input_aggregation.hpp"
#include "game/sim/player_sim_config.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "oge/assert.hpp"
#include "oge/platform/io.hpp"

using namespace game;

Scene::Scene(const Def& def)
    : AppRuntime(def.ctx),
      // The fixed pipeline ticks once per sub-step (D2): Scene::Update
      // drives the sub-steps explicitly and the stages observe them via
      // SimTickContext.subStepIdx.  A longer interval would let the
      // pipeline's internal scheduler re-accumulate the sub-steps and
      // collapse them into one update at the last sub-step index — the
      // subStepIdx == 0 decision gate would never open and the fixed
      // stages would never decide anything.  (The transport scenes call
      // SetUpdateInterval(sim::kSubStepDt) explicitly; this default keeps
      // the bare scene on the same cadence.)
      m_subsystems({m_world, def.ctx.events, def.ctx.memory},
                   sim::kSubStepDt),
      m_realtimeSubsystems({m_world, def.ctx.events, def.ctx.memory})
{
    // The sim always runs in a tick space — the transport layer is
    // optional, never required.  Every scene world gets its SimTickContext
    // at construction (not in Load): the scene runner constructs a scene
    // and Updates it without ever loading it (e.g. ClientConnScene, the
    // bare Scene placeholder), and Scene::Update's fixed block reads the
    // tick unconditionally.  Transport scenes overwrite currentTick every
    // Update from their replication tick; a scene that owns its tick starts
    // at 0 and advances it in Update (tick arbitration).  emplace is
    // idempotent (try_emplace), so the transport scenes' own emplace in
    // their constructors just returns this one.
    m_world.ctx().emplace<sim::SimTickContext>();

    auto it = def.args.find("scene_config");
    if (it != def.args.end())
    {
        m_sceneConfig = json::FromJson<SceneConfig>(it->second);
    }
}

Scene::~Scene()
{
}

void Scene::Update(Frame f, SceneContext sctx)
{
    m_ctx.memory.Update(f.dt);

    // Scene-driven fixed-step loop (D2): accumulate render dt and execute
    // the fixed pipeline as sub-steps of sim::kSubStepDt once the fixed
    // frame is due.  The sub-step boundaries are visible to the stages via
    // SimTickContext.subStepIdx (0 = decisions, 1.. = re-application).
    // kFixedEps covers the harness's POLL_DT (0.016) vs 1/60 drift.
    constexpr float kFixedEps = 0.005f;
    m_fixedAccum += f.dt;
    if (m_fixedAccum + kFixedEps >= m_fixedFrameDuration)
    {
        const int subSteps =
            (int)std::lround(m_fixedFrameDuration / sim::kSubStepDt);
        OGE_ASSERT(subSteps >= 1,
                   "fixed frame duration {} is shorter than one sub-step {}",
                   m_fixedFrameDuration, sim::kSubStepDt);
        auto& tickCtx = m_world.ctx().get<sim::SimTickContext>();
        // Tick arbitration: transport scenes advance currentTick before
        // Update (their tick space is the replication tick).  When the tick
        // is unchanged, this scene owns its tick space — advance it and
        // aggregate the local input into tick-stamped frames, exactly like
        // the transport pollers do.  The same stamp (T - pipeline delay)
        // feeds the same fixed-stage code path in every configuration.
        if (tickCtx.currentTick == m_lastFixedTick)
        {
            ++tickCtx.currentTick;
            sim::AggregateLocalInputs(
                m_world,
                tickCtx.currentTick - input::kInputPipelineDelayTicks);
        }
        m_lastFixedTick = tickCtx.currentTick;
        for (int s = 0; s < subSteps; ++s)
        {
            tickCtx.subStepIdx = (uint8_t)s;
            m_subsystems.Update(sim::kSubStepDt);
        }
        m_fixedAccum = 0.f;
    }

    m_realtimeSubsystems.Update(f.dt);
}

void Scene::Load()
{
#if OGE_DEBUG
    OGE_ASSERT(m_lifetimeStage != LifetimeStage::Loaded, "double load");
    m_lifetimeStage = LifetimeStage::Loaded;
#endif
    if (m_sceneConfig.loadMask & SceneConfig::LOAD_MASK_BLOCKS)
    {
        auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
        for (const auto& [name, config] : m_sceneConfig.blocks)
        {
            blocks.RegisterBlock(std::move(std::string(name)), config);
        }
    }
    if (m_sceneConfig.loadMask & SceneConfig::LOAD_MASK_TERRAIN)
    {
        m_world.ctx().emplace<::game::terrain::TerrainDesc>(
            m_sceneConfig.terrainDesc);
        m_world.ctx().emplace<::game::terrain::TerrainView>();
    }
    for (auto stage : m_sceneConfig.subsystems)
    {
        m_subsystems.AddStage(m_ctx.any_factory, stage);
    }
    for (auto stage : m_sceneConfig.realtimeSubsystems)
    {
        m_realtimeSubsystems.AddStage(m_ctx.any_factory, stage);
    }
}

void Scene::Unload()
{
#if OGE_DEBUG
    OGE_ASSERT(m_lifetimeStage != LifetimeStage::Unloaded, "double unload");
    m_lifetimeStage = LifetimeStage::Unloaded;
#endif
    m_subsystems.Clear();
    m_realtimeSubsystems.Clear();
}

namespace game
{
SceneConfig GetDefaultSceneConfig(AnythingFactory& af)
{
    SceneConfig config{};
    config.loadMask = SceneConfig::LOAD_MASK_BLOCKS | SceneConfig::LOAD_MASK_TERRAIN;
    config.blocks = {
        {"dirt",
         {
             "Dirt",
             "dirt.png",
             1,
         }},
        {"wood", {"Wood", "wood_plank.png", 1}},
        {"stone", {"Stone", "green_stone.png", 1}},
    };
    config.terrainDesc.chunkViewDistance = 4;

    config.subsystems.push_back(af.Id<sim::SubsystemTerrain>());
    config.subsystems.push_back(
        af.Id<sim::SubsystemPlayer<UpdateType::FixedStep>>());
    config.subsystems.push_back(
        af.Id<sim::SubsystemCreature<UpdateType::FixedStep>>());
    config.subsystems.push_back(
        af.Id<sim::SubsystemPhysics<UpdateType::FixedStep>>());

    config.realtimeSubsystems.push_back(
        af.Id<sim::SubsystemPlayer<UpdateType::Realtime>>());
    config.realtimeSubsystems.push_back(
        af.Id<sim::SubsystemCreature<UpdateType::Realtime>>());
    config.realtimeSubsystems.push_back(
        af.Id<sim::SubsystemPhysics<UpdateType::Realtime>>());
    return config;
}

PlayerInfo LoadOrCreatePlayer()
{
    std::vector<char> data;
    if (!oge::platform::TryLoadBlob("player.bin", data))
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        uuids::uuid const id = uuids::uuid_random_generator{gen}();
        data.resize(16 + sizeof(math::vec3));
        memcpy(data.data(), id.as_bytes().data(), id.as_bytes().size_bytes());
        math::vec3 pos{20.f, 20.f, 20.f};
        memcpy(&data[16], &pos, sizeof(math::vec3));
        oge::platform::TrySaveBlob("player.bin", data);
    }
    assert(data.size() == 16 + sizeof(math::vec3));
    std::array<uint8_t, 16> _uuid;
    memcpy(_uuid.data(), data.data(), 16);
    math::vec3 pos;
    memcpy(&pos, &data[16], sizeof(math::vec3));
    LOG_INFO("player loaded with uuid {}",
             uuids::to_string(uuids::uuid{_uuid}));
    return {std::move(_uuid), pos};
}
}  // namespace game
